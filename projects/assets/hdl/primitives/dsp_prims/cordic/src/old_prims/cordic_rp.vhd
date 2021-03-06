-- This file is protected by Copyright. Please refer to the COPYRIGHT file
-- distributed with this source distribution.
--
-- This file is part of OpenCPI <http://www.opencpi.org>
--
-- OpenCPI is free software: you can redistribute it and/or modify it under the
-- terms of the GNU Lesser General Public License as published by the Free
-- Software Foundation, either version 3 of the License, or (at your option) any
-- later version.
--
-- OpenCPI is distributed in the hope that it will be useful, but WITHOUT ANY
-- WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
-- A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
-- details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this program. If not, see <http://www.gnu.org/licenses/>.

-------------------------------------------------------------------------------
-- CORDIC : Rectangular-to-Polar Conversion
-------------------------------------------------------------------------------
--
-- File: cordic_rp.vhd
--
-- Description:
--
-- CORDIC Function Implemented: Rectangular-to-Polar Conversion
--
-- Angle Mode = Circular
-- Direction Mode = Vectoring
-- Sequence = 0 to STAGES-1
-- Microrotation Angles: sigma(i) = atan(2^(-i))
-- Gain Correction Factor: Kc = prod(cos(sigma(i)))
--
-- Inputs:
--   X0 = Kc * I
--   Y0 = Kc * Q
--   Z0 = 0
--
-- Results:
--   XN = Mag   = sqrt( I^2 + Q^2 )
--   ZN = Phase = atan( Q / I )
--
-- Domain:
--   X0  >  0
--   X0  <  2
--  |Y0| <= 2
--
-- Input Adjustments:
--
-- The CORDIC will only converge in this mode if the I component of the input
-- vector is positive. So if it is negative, we negate it and keep track of that
-- fact so that a compensating adjustment can be made to the corresponding output.
-- This is handled by the flip_dly vector. The input adjustment can be thought
-- of as a simple mirroring from quadrant 2 to 1; and from quadrant 3 to 4.
--
-- Output adjustments:
--
-- The input has had it's I component negated if flip_dly(STAGES) = 1. When this
-- is the case we adjust the phase output of the CORDIC to mirror the output
-- vector back into its original quadrant. If the phase is positive we mirror from
-- quadrant 1 to 2. If it is negative we mirror from quadrant 4 to 3.
--
-- When the CORDIC input I component has been flipped, look at the CORDIC output
-- phase and adjust as follows:
--
--   CORDIC Out Phase (p)   Final Output Phase
--   ====================   ==================
--         >= 0                  pi - p
--         < 0                  -pi - p
--
-- Output Scaling:
--
-- The output Phase of the CORDIC, prior to the adjustment above will range
-- from -pi/2 to +pi/2 scaled by 2^(DATA_WIDTH-2). To account for the doubling
-- of the range (-pi to +pi) we will right shift by one bit and then perform
-- the pi-p and -pi-p math using pi*2^(DATA_WIDTH-3).
--
-- The result will be a phase that extends from -pi to +pi scaled by 2^(DATA_WIDTH-3).
-- This will be scaled by a constant factor of 4/pi so that a maximum positive
-- value of 2^(DATA_WIDTH-1)-1 corresponds to (almost) +pi. And the maximum
-- negative value of 2^(DATA_WIDTH-1) will correspond to -pi.
--
-------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all;

entity cordic_rp is
generic (
   DATA_WIDTH  : positive;
   STAGES      : positive);

port (
   CLK      : in std_logic;
   RST      : in std_logic;
   I        : in std_logic_vector(DATA_WIDTH-1 downto 0);
   Q        : in std_logic_vector(DATA_WIDTH-1 downto 0);
   VLD_IN   : in std_logic;
   MAG      : out std_logic_vector(DATA_WIDTH-1 downto 0);
   PHASE    : out std_logic_vector(DATA_WIDTH-1 downto 0);
   VLD_OUT  : out std_logic);

end cordic_rp;

architecture behavior of cordic_rp is

   component cordic
   generic (
      DATA_WIDTH  : positive;
      STAGES      : positive;
      ANGLE_MODE  : string  ;
      DIR_MODE    : string  );
   port (
      CLK      : in std_logic;
      RST      : in std_logic;
      X_IN     : in std_logic_vector(DATA_WIDTH-1 downto 0);
      Y_IN     : in std_logic_vector(DATA_WIDTH-1 downto 0);
      Z_IN     : in std_logic_vector(DATA_WIDTH-1 downto 0);
      VLD_IN   : in std_logic;
      X_OUT    : out std_logic_vector(DATA_WIDTH-1 downto 0);
      Y_OUT    : out std_logic_vector(DATA_WIDTH-1 downto 0);
      Z_OUT    : out std_logic_vector(DATA_WIDTH-1 downto 0);
      VLD_OUT  : out std_logic);
   end component;

   constant ZERO          : std_logic_vector(DATA_WIDTH-1 downto 0) := (others => '0');
   constant PI            : std_logic_vector(DATA_WIDTH-1 downto 0) := std_logic_vector(to_signed(integer(2.0**(DATA_WIDTH-3)*(MATH_PI)),DATA_WIDTH));
   constant NEG_PI        : std_logic_vector(DATA_WIDTH-1 downto 0) := std_logic_vector(to_signed(integer(2.0**(DATA_WIDTH-3)*(-MATH_PI)),DATA_WIDTH));
   constant ONE_OVER_PI   : std_logic_vector(DATA_WIDTH-1 downto 0) := std_logic_vector(to_signed(integer(2.0**(DATA_WIDTH-1)*(1.0/MATH_PI)),DATA_WIDTH));

   signal K                 : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal ik_pre            : std_logic_vector(2*DATA_WIDTH-1 downto 0);
   signal qk_pre            : std_logic_vector(2*DATA_WIDTH-1 downto 0);
   signal ik                : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal qk                : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal ik_neg            : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal ik_adj            : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal ik_reg            : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal qk_reg            : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal vld_in_reg        : std_logic;
   signal vld_out_pre       : std_logic;
   signal flip_pre          : std_logic;
   signal flip              : std_logic;
   signal flip_dly          : std_logic_vector(STAGES downto 0);
   signal mag_pre           : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal pha_pre           : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal pha_rsh           : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal pi_minus_pha      : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal neg_pi_minus_pha  : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal pha_adj           : std_logic_vector(DATA_WIDTH-1 downto 0);
   signal pha_scaled_long   : std_logic_vector(2*DATA_WIDTH-1 downto 0);
   signal pha_scaled        : std_logic_vector(DATA_WIDTH-1 downto 0);

begin

   ----------------------------------------------------------------------------
   -- Calculate K for Circular Angle Mode
   ----------------------------------------------------------------------------

   -- The value K is used to correct for CORDIC gain. It depends on the user
   -- parameters: STAGES, DIR_MODE, and ANGLE_MODE. The process below
   -- calculates K when ANGLE_MODE is CIRCULAR. Since it depends only upon
   -- static values it will not result in an infinite loop for simulation. And
   -- for synthesis it will reduce to a static value assignment for K with no
   -- logic inferred.

   k_cir_proc: process
      variable k_real : real;
   begin
      k_real := 1.0;
      for i in 0 to STAGES-1 loop
         k_real := k_real * cos(arctan(2.0**(-i)));
      end loop;
      K <= std_logic_vector(to_signed(integer(floor((2.0**(DATA_WIDTH-2)-1.0) * k_real)),DATA_WIDTH));
      wait;
   end process;

   ----------------------------------------------------------------------------
   -- Prescale I and Q by K
   ----------------------------------------------------------------------------

   ik_pre <= std_logic_vector(signed(I) * signed(K));
   qk_pre <= std_logic_vector(signed(Q) * signed(K));

   -- Since Kc has two sign bits to the left of the radix point, our result
   -- will actually have three sign bits. Grab the appropriate slice for the
   -- result to keep the width at DATA_WIDTH. May want to perform convergent
   -- round on LSB for improved results in future.

   ik <= ik_pre(2*DATA_WIDTH-3 downto DATA_WIDTH-2);
   qk <= qk_pre(2*DATA_WIDTH-3 downto DATA_WIDTH-2);

   ----------------------------------------------------------------------------
   -- Guarantee that I >= 0 for CORDIC Convergence
   -- Keep track of flip with "flip" tag
   ----------------------------------------------------------------------------

   ik_neg <= std_logic_vector(-signed(ik));
   ik_adj <= ik_neg when (signed(ik) < 0) else ik;
   flip_pre <= '1' when (signed(ik) < 0) else '0';

   ----------------------------------------------------------------------------
   -- Register all the above results before feeding into CORDIC
   -- May need additional pipelining for higher clock rates
   ----------------------------------------------------------------------------

   process(CLK)
   begin
      if rising_edge(CLK) then
         ik_reg <= ik_adj;
         qk_reg <= qk;
         vld_in_reg <= VLD_IN;
         flip <= flip_pre;
      end if;
   end process;

   ----------------------------------------------------------------------------
   -- Create a delay line for "flip" to match prop delay of CORDIC
   ----------------------------------------------------------------------------

   flip_dly(0) <= flip;

   gen_flip_dly: for i in 0 to STAGES-1 generate
      flip_ff: process(CLK)
      begin
         if rising_edge(CLK) then
            flip_dly(i+1) <= flip_dly(i);
         end if;
      end process;
   end generate;

   ----------------------------------------------------------------------------
   -- Instantiate the CORDIC
   ----------------------------------------------------------------------------

   cordic_inst: cordic
   generic map (
      DATA_WIDTH  => DATA_WIDTH,
      STAGES      => STAGES,
      ANGLE_MODE  => "CIRCULAR",
      DIR_MODE    => "VECTORING")
   port map (
      CLK      => CLK,
      RST      => RST,
      X_IN     => ik_reg,
      Y_IN     => qk_reg,
      Z_IN     => ZERO,
      VLD_IN   => vld_in_reg,
      X_OUT    => mag_pre,
      Y_OUT    => open,
      Z_OUT    => pha_pre,
      VLD_OUT  => vld_out_pre);

   ----------------------------------------------------------------------------
   -- CORDIC phase output is scaled by 2^(DATA_WIDTH-2)
   -- Right shift by 1 bit to prepare for math with PI and NEG_PI which are
   -- scaled by 2^(DATA_WIDTH-3)
   ----------------------------------------------------------------------------

   pha_rsh <= std_logic_vector(shift_right(signed(pha_pre),1));

   ----------------------------------------------------------------------------
   -- Adjust output Phase if Input I component was fliped at CORDIC input
   ----------------------------------------------------------------------------

   pi_minus_pha <= std_logic_vector(signed(PI) - signed(pha_rsh));
   neg_pi_minus_pha <= std_logic_vector(signed(NEG_PI) - signed(pha_rsh));

   process (pi_minus_pha, neg_pi_minus_pha, pha_rsh, flip_dly(STAGES))
   begin
      if flip_dly(STAGES) = '1' then
         if (signed(pha_pre) < 0) then
            pha_adj <= neg_pi_minus_pha;
         else
            pha_adj <= pi_minus_pha;
         end if;
      else
         pha_adj <= pha_rsh;
      end if;
   end process;

   ----------------------------------------------------------------------------
   -- The adjusted CORDIC Output Phase now ranges from -pi to +pi scaled by
   -- 2^(DATA_WIDTH-3). Scaled this by 4/pi so it ranges from max negative
   -- to max positive.
   ----------------------------------------------------------------------------

   -- Scale by 1/pi
   pha_scaled_long <= std_logic_vector(signed(pha_adj) * signed(ONE_OVER_PI));

   -- Grab the result and shift left by 2 bits to scale by 4
   pha_scaled <= pha_scaled_long(2*DATA_WIDTH-4 downto DATA_WIDTH-3);

   ----------------------------------------------------------------------------
   -- Register results before driving out
   -- May need additional pipelining for higher clock rates
   ----------------------------------------------------------------------------

   process(CLK)
   begin
      if rising_edge(CLK) then
         MAG <= mag_pre;
         PHASE <= pha_scaled;
         VLD_OUT <= vld_out_pre;
      end if;
   end process;

end behavior;

