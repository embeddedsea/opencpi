#!/usr/bin/env python3
# This file is protected by Copyright. Please refer to the COPYRIGHT file
# distributed with this source distribution.
#
# This file is part of OpenCPI <http://www.opencpi.org>
#
# OpenCPI is free software: you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# OpenCPI is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License along
# with this program. If not, see <http://www.gnu.org/licenses/>.

# TODO: integrate more inline with ocpirun -A to get information instead of metadata file

import argparse
import subprocess
import os
import sys
import json
import types
import pydoc
from xml.etree import ElementTree as ET
import ocpiutil
import ocpiassets
from hdltargets import HdlTarget, HdlPlatform
from ocpiassets import Registry

subParserNouns = ["hdl", "rcc"]
plainNouns = ["registry", "projects", "workers", "components", "platforms", "targets", "tests", 
              "libraries", "project"]
nounsList = plainNouns + subParserNouns
subnouns = {}
subnouns['hdl'] = ["targets", "platforms"]
subnouns['rcc'] = ["targets", "platforms"]
def parseCLVars():
    description = "Utility for showing the project registry, any " + \
                  "available projects (registered or in path), workers, " + \
                  "components, HDL/RCC Platforms and Targets available at built time."
    parser = argparse.ArgumentParser(description=description)
    # Print help screen and usage to pager. Code block adapted and inlined from from:
    # https://github.com/fmenabe/python-clg/blob/master/LICENSE#L6
    #     under MIT license
    parser.print_help = types.MethodType(lambda self, _=None: pydoc.pager("\n" +
                                                                          self.format_help()),
                                         parser)
    parser.print_usage = types.MethodType(lambda self, _=None: pydoc.pager("\n" +
                                                                           self.format_usage()),
                                          parser)
    # This displays the error AND help screen when there is a usage error or no arguments provided
    parser.error = types.MethodType(
        lambda self, error_message: (
            pydoc.pager(error_message + "\n\n" + self.format_help()),
            exit(1)
            ), parser)

    parser.add_argument("noun", type=str, help="This is either the noun to show or the " +
                        "authoring model to operate on. If choosing an authoring model " +
                        "(hdl/rcc), the platforms or targets nouns can follow.",
                        choices=nounsList)
    parser.add_argument("-v", "--verbose", action="store_true", help="Be verbose with output.")
    parser.add_argument("-d", dest="dir", default=os.path.curdir,
                        help="Change directory to the specified path " + "before proceeding. " +
                        "Changing directory may have no effect for some commands.")

    details_group = parser.add_mutually_exclusive_group()
    details_group.add_argument("--table", action="store_const", dest="details", const="table",
                               default="table",
                               help="Pretty-print details in a table associated with the chosen " 
                               "noun.  This is the default if no print mode is specified")
    details_group.add_argument("--json", action="store_const", dest="details", const="json",
                               default="table",
                               help="Print information in a json format")
    details_group.add_argument("--simple", action="store_const", dest="details", const="simple",
                               default="table",
                               help="Print information in a simple format ")
    scope_group = parser.add_mutually_exclusive_group()
    scope_group.add_argument("--local-scope", action="store_const", dest="scope", const="local",
                             default="global",
                             help="Only show assets in the local project")
    #scope_group.add_argument("--available-scope", action="store_const", dest="scope", const="depend",
    #                         default="global",
    #                         help="Only show assets in the local project and the projects it " +
    #                             "depends on")
    scope_group.add_argument("--global-scope", action="store_const", dest="scope", const="global",
                             default="global",
                             help="show assets in all projects. This is the default if no "+
                                  "scope mode is given")

    first_pass_args, remaining_args = parser.parse_known_args()
    if first_pass_args.noun in plainNouns:
        args = first_pass_args
        noun = first_pass_args.noun
    elif first_pass_args.noun in subParserNouns:
        subparser = argparse.ArgumentParser(description=description)

        subparser.add_argument("subnoun", type=str, help="This is either the noun to show or the " +
                               "authoring model to operate on. If choosing an authoring model " +
                               "(hdl/rcc), the platforms or targets noun can follow.",
                               choices=subnouns[first_pass_args.noun])
        args = subparser.parse_args(remaining_args, namespace=first_pass_args)
        noun = first_pass_args.noun + args.subnoun

    return args, noun

# TODO: Consider using python's pprint instead of json.dump for multi-line indented JSON print instead
#       of single line

def do_rccplatforms(options):
    """
    Print out the list of available RccPlatforms
    Without options this will just be a list.
    If the "table" option is provided, print out a formatted table of the platforms and targets.
    If the "json" option is provided, dump json output for the platform/target details.
    """
    rccDict = ocpiutil.get_make_vars_rcc_targets()
    try:
      rccPlatforms = rccDict["RccAllPlatforms"]
      rccTargets   = rccDict["RccAllTargets"]
    except TypeError:
        print("No RCC platforms found. Make sure the core project is registered or in your OCPI_PROJECT_PATH.")
        return
    if options.details == "table":
        rows = [["Platform", "Target"]]
        rows += [["---------", "---------"]]
        # Collect the information for each platform into a "row" list
        for platform in sorted(rccPlatforms):
            # Assuming there is only one target per platform with [0]
            target = rccDict["RccTarget_" + platform][0]
            platformRow = [platform, target]
            rows.append(platformRow)
        # Format the list of rows into a table and print
        ocpiutil.print_table(rows)
    elif options.details in {"json", "table"}:
        platformDict = {}
        for platform in sorted(rccPlatforms):
            target = rccDict["RccTarget_" + platform][0]
            platformDict[platform] = {"target": target}
        json.dump(platformDict, sys.stdout)
        print()
    else:
        print(ocpiutil.python_list_to_bash(sorted(rccDict["RccAllPlatforms"])))

def do_rcctargets(options):
    """
    Print out the list of available RccTargets
    Without options this will just be a list.
    If the "table" option is provided, print out a formatted table of the platforms and targets.
    If the "json" option is provided, dump json output for the platform/target details.
    """
    if options.details in {"json", "table"}:
        do_rccplatforms(options)
    else:
        rccDict = ocpiutil.get_make_vars_rcc_targets()
        try:
          print(ocpiutil.python_list_to_bash(sorted(rccDict["RccAllTargets"])))
        except TypeError:
            print("No RCC targets found. Make sure the core project is registered or in your OCPI_PROJECT_PATH.")
            return


def do_hdlplatforms(options):
    """
    Print out the list of available HdlPlatforms.
    Without options, this will just be a list.
    If the "table" option is provided, print out a formatted table of the platforms an
    the details associated with each.
    If the "json" option is provided, dump json output for the platform details.
    """
    platforms = HdlPlatform.all()
    if options.details == "table":
        rows = [["Platform", "Target", "Part", "Vendor", "Toolset"]]
        rows += [["---------", "---------", "---------", "---------", "---------"]]
        # Collect the information for each platform into a "row" list
        for platform in HdlPlatform.all():
            target = platform.target
            platformStr = platform.name
            if not platform.built:
                platformStr += '*'
            platformRow = [platformStr, str(target), platform.exactpart,
                           target.vendor, target.toolset.title]
            rows.append(platformRow)
        # Format the list of rows into a table and print
        ocpiutil.print_table(rows)
        print("* An asterisk indicates that the platform has not been built yet.\n" + \
              "  Assemblies and tests cannot be built until the platform is built.")
    elif options.details == "json":
        # Dump the json containing each platform's attributes
        platformDict = {}
        for platform in HdlPlatform.all():
            target = platform.target
            platformDict[platform.name] = {"target": target.name,
                                           "part": platform.exactpart,
                                           "vendor": target.vendor,
                                           "tool": target.toolset.title,
                                           "built": platform.built}
        json.dump(platformDict, sys.stdout)
        print()
    else:
        print(ocpiutil.python_list_to_bash(sorted(platforms)))

def do_hdltargets(options):
    """
    Print out the list of available HdlTargets.
    Without options, this will just be a list.
    If the "dense" option is provided, print out an ugly but parsable list of targets and
    the details associated with each.
    If the "table" option is provided, print out a formatted table of the targets an
    the details associated with each.
    If the "json" option is provided, dump json output for the target details.
    """
    if options.details == "table":
        rows = [["Target", "Parts", "Vendor", "Toolset"]]
        rows += [["---------", "---------", "---------", "---------"]]
        # Collect the information for each target into a "row" list
        for target in HdlTarget.all():
            targetRow = [str(target), ','.join(target.parts), target.vendor, target.toolset.title]
            rows.append(targetRow)
        ocpiutil.print_table(rows)
    elif options.details == "json":
        # Dump the json containing each platform's attributes
        vendorDict = {}
        for vendor in HdlTarget.get_all_vendors():
            targetDict = {}
            for target in HdlTarget.get_all_targets_for_vendor(vendor):
                targetDict[target.name] = {"parts": target.parts,
                                           "tool": target.toolset.title}
            vendorDict[vendor] = targetDict
        json.dump(vendorDict, sys.stdout)
        print()
    else:
        print(ocpiutil.python_list_to_bash(sorted([str(tgt) for tgt in HdlTarget.all()])))

def do_platforms(options):
    """
    Print out platforms for all authoring models
    """
    if options.details != "json":
        print("RCC:")
    else:
        print("{")
        print("\"rcc\":")
    do_rccplatforms(options)
    if options.details != "json":
        print("HDL:")
    else:
        print(",")
        print("\"hdl\":")
    do_hdlplatforms(options)
    if options.details == "json":
        print("}")

def do_targets(options):
    """
    Print out targets for all authoring models
    """
    if options.details != "json":
        print("RCC:")
    do_rcctargets(options)
    if options.details != "json":
        print("HDL:")
    do_hdltargets(options)

def do_worker_or_comp(options, worker):
    #TODO this is not be the right way to do this long term
    if worker:
        cap_asset = "Worker"
        xml_asset = "worker"
    else:
        cap_asset = "Component"
        xml_asset = "spec"
    mdFileList = ocpiutil.get_all_projects()
    assetList = ""
    if options.details == "table":
        # Table header
        row_1 = ["Project Directory", cap_asset]
        row_2 = ["------------------", "------------------------------------"]
        rows = [row_1, row_2]

    for proj_dir in mdFileList:
        proj_dir = rchop(proj_dir, '/')
        proj_dir = rchop(proj_dir, 'exports')
        if os.access('proj_dir', os.W_OK):
            subprocess.check_output([os.environ['OCPI_CDK_DIR']+"/scripts/genProjMetaData.py", mdFile])
        mdFile = proj_dir + "/project.xml"
        if os.path.isfile(mdFile):
            assetList = get_tags(mdFile, xml_asset)
            if options.details == "simple":
                print("Project: " + proj_dir)
                for asset in assetList.split():
                    print("    " + cap_asset + ": " + asset)
            elif options.details == "table":
                # Generate rows of the table. Only add the registered column if only_registry is False
                for asset in assetList.split():
                    row_n = [os.path.realpath(proj_dir), asset]
                    rows.append(row_n)
            else:
                raise ocpiutil.OCPIException("ocpidev show components/worker --json is not " +
                                             "valid, this option is under construction.")
    if options.details == "table":
        ocpiutil.print_table(rows)

def do_components(options):
    if options.scope != "global":
        raise ocpiutil.OCPIException("ocpidev show components is only valid in \"--global-scope\"." +
                                     "  Other scope options are under construction.")
    do_worker_or_comp(options, False)

def do_workers(options):
    if options.scope != "global":
        raise ocpiutil.OCPIException("ocpidev show workers is only valid in \"--global-scope\"." +
                                     "  Other scope options are under construction.")
    do_worker_or_comp(options, True)

def do_libraries(options):
    if options.scope != "local":
        raise ocpiutil.OCPIException("ocpidev show libraries is only valid in \"--local-scope\"." +
                                     "  Other scope options are under construction.")
    if not ocpiutil.get_path_to_project_top():
        raise ocpiutil.OCPIException("When using the \"--local-scope\" scope you must run the command " +
                                    "in a valid OpenCPI project. " + os.path.realpath(".") +
                                    " is not a valid project.  Use the \"-d\" option or change " +
                                    "directories")
    #TODO this may not be the right way to do this long term
    my_asset = ocpiassets.AssetFactory.factory("project",
                                               ocpiutil.get_path_to_project_top())
    my_asset.show(libraries=True, details=options.details, verbose=options.verbose)

def do_project(options):
    #TODO this may not be the right way to do this long term
    if options.verbose:
        my_asset = ocpiassets.AssetFactory.factory("project",
                                               ocpiutil.get_path_to_project_top(),
                                               init_libs=True)
    else:
        my_asset = ocpiassets.AssetFactory.factory("project",
                                               ocpiutil.get_path_to_project_top())
    my_asset.show(details=options.details, verbose=options.verbose)

def do_tests(options):
    if options.scope != "local":
        raise ocpiutil.OCPIException("ocpidev show tests is only valid in \"--local\" scope.  " +
                                     "Other scope options are under construction.")
    if not ocpiutil.get_path_to_project_top():
        raise ocpiutil.OCPIException("When using the \"--local\" scope you must run the command " +
                                    "in a valid OpenCPI project. " + os.path.realpath(".") +
                                    " is not a valid project.  Use the \"-d\" option or change " +
                                    "directories")
    #TODO this may not be the right way to do this long term
    my_asset = ocpiassets.AssetFactory.factory("project",
                                               ocpiutil.get_path_to_project_top(),
                                               init_libs=True)
    my_asset.show(tests=True, details=options.details, verbose=options.verbose)

def rchop(thestring, ending):
    if thestring.endswith(ending):
        return thestring[:-len(ending)]
    return thestring

def get_tags(mdFile, tagName):
    #print "File is: " + mdFile
    retVal = ""
    if (os.path.isfile(mdFile)):
        tree = ET.parse(mdFile)
        for tag in tree.iter(tagName):
          retVal = retVal + " " + tag.get("name")
    return retVal

def do_registry(options):
    """
    Print out the currently registered projects
        -- different from do_projects because we omit OCPI_PROJECT_PATH
    """
    do_projects(options, only_registry=True)

def collect_projects_from_path():
    project_path = os.environ.get('OCPI_PROJECT_PATH')
    projects_from_env = {}
    if not project_path is None and not project_path.strip() == "":
        projects_from_path = project_path.split(':')
        for proj in projects_from_path:
            proj_package = ocpiutil.get_project_package(proj)
            if proj_package is None:
                proj_package = os.path.basename(proj.rstrip("/"))
                proj_exists = False
            else:
                proj_exists = True
            projects_from_env[proj_package] = {}
            projects_from_env[proj_package]["exists"] = proj_exists
            projects_from_env[proj_package]["registered"] = False
            projects_from_env[proj_package]["real_path"] = os.path.realpath(proj)
    return projects_from_env

def do_projects(options, only_registry=False):
    """
    Print out the currently registered projects and optionally those in project path
    JSON format gives:
    {
      "registry_location":  <registry_directory>,
      "projects": {
                    <project-ID>: {
                                   "link_contents": <contents-of-link>,
                                   "real_path"    : <real-path-of-project>
                                  }
                  }
    }
    """
    # Get projects from registry
    try:
        reg_dir = Registry.get_registry_dir()
        projects = os.listdir(reg_dir)
        reg_exists = True
    except ocpiutil.OCPIException as err:
        print(err)
        reg_exists = False

    # If we care about projects from other sources (e.g. OCPI_PROJECT_PATH),
    # collect those too
    if not only_registry:
        projects_from_env = collect_projects_from_path()

    if options.details == "simple":
        # With no options, just print the result of 'ls <registry>'
        # omit broken links
        proj_list = []
        # List the projects in registry that exist (unbroken links)
        if reg_exists:
            proj_list += [proj for proj in os.listdir(reg_dir) if os.path.exists(reg_dir + "/" +
                                                                                 proj)]
        # If we care about other sources of projects (e.g. OCPI_PROJECT_PATH),
        # include those as well
        if not only_registry:
            proj_list += [proj for proj in projects_from_env]
        print(ocpiutil.python_list_to_bash(sorted(proj_list)))
    else:
        # Collect a dictionary mapping registered project link to its contents
        projects_dict = {}
        # Loop through registered projects and generate dictionary segments.
        #     Check if the project exists (is the link broken?)
        #     If not in only_registry mode, set registered=True
        if reg_exists:
            for proj in projects:
                proj_path = reg_dir + "/" + proj
                if os.path.islink(proj_path):
                    projects_dict[proj] = {}
                    # Get the real path pointed to by the link
                    project_path = os.path.realpath(proj_path)
                    projects_dict[proj]["real_path"] = project_path
                    # Flag for whether the link points to an existing loations
                    projects_dict[proj]["exists"] = os.path.exists(project_path) and \
                                                    os.path.isdir(project_path)
                    # Flag for whether the link points to an existing loations
                    if not only_registry:
                        projects_dict[proj]["registered"] = True

        # Include the project dictionary segments from other sources
        if not only_registry:
            projects_dict.update(projects_from_env)

        if options.details == "table":
            # Print out a table listing project IDs, projects' realpaths, and whether they exist
            if reg_exists:
                print("Project registry is located at: " + reg_dir)
            else:
                print("There is no project registry, or it does not exist.")

            # Table header
            row_1 = ["Project Package-ID", "Path to Project", "Valid/Exists"]
            row_2 = ["------------------", "---------------", "------------"]

            # No reason to output the 'Registered' column if the noun is registry
            if not only_registry:
                row_1.append("Registered")
                row_2.append("----------")
            rows = [row_1, row_2]

            # Generate rows of the table. Only add the registered column if only_registry is False
            for link, dest in projects_dict.items():
                row_n = [link, dest["real_path"], dest["exists"]]
                if not only_registry:
                    row_n.append(dest["registered"])
                rows.append(row_n)
            ocpiutil.print_table(rows)
        else:
            # JSON mode
            # Print registry location and project links in JSON format
            project_reg_dict = {}
            if reg_exists:
                project_reg_dict["registry_location"] = reg_dir
            project_reg_dict["projects"] = projects_dict
            json.dump(project_reg_dict, sys.stdout)
            print()

def main():
    ocpiutil.configure_logging()
    (args, noun) = parseCLVars()
    action = {"registry":     do_registry,
              "projects":     do_projects,
              "project":      do_project,
              "components":   do_components,
              "workers":      do_workers,
              "tests":        do_tests,
              "libraries":    do_libraries,
              "platforms":    do_platforms,
              "targets":      do_targets,
              "hdlplatforms": do_hdlplatforms,
              "hdltargets":   do_hdltargets,
              "rccplatforms": do_rccplatforms,
              "rcctargets":   do_rcctargets}
    if args.verbose and args.details != "json":
        #TODO use ocpilogging module
        print("ocpishow is processing noun \"" + str(noun) + "\" with options \"" + str(args) + "\"")
    with ocpiutil.cd(args.dir):
        # call the correct noun function
        try:
            action[noun](args)
        except ocpiutil.OCPIException as ex:
            ocpiutil.logging.error(str(ex))
            sys.exit(1)

if __name__ == '__main__':
    main()
