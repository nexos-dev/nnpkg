/*
    main.c - contains entry point to nnpkg
    Copyright 2022 The NexNix Project

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "include/nnpkg.h"
#include <assert.h>
#include <config.h>
#ifdef NNPKG_ENABLE_NLS
#include <libintl.h>
#endif
#include <libnex.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

// Action to execute
static action_t* action = NULL;

// Action table. This table contains entries for each action, specifying the name of
// the action, a function to obtain the actions argument table, and a function to run
// actions
static action_t actions[] = {
    {"init", initGetOptions, initRunAction},
    {"add",  addGetOptions,  addRunAction }
};

// Runs an argument specified
bool parseArg (char** argv, int* i, actionOption_t* opt)
{
    // Find an argument to it
    char* arg = NULL;
    if (argv[(*i) + 1] && argv[(*i) + 1][0] != '-')
    {
        arg = argv[*i + 1];
        // ENsure loop doesn't pick up argument
        *i = *i + 1;
    }
    // Check if an argument must be passed
    if (opt->argRequired && !arg)
    {
        error ("option \"%s\" requires an argument", argv[*i]);
        return false;
    }
    if (!opt->parse (opt, arg))
        return false;
    return true;
}

// Parses action arguments
bool parseAction (int argc, char** argv)
{
    assert (action);
    // Get option table
    actionOption_t* opts = action->getOptTable();
    actionOption_t* optsBase = opts;
    // Loop through argv, then loop through option table to find option in argv
    for (int i = 0; i < argc; ++i)
    {
        // Reset state
        opts = optsBase;
        if (!argv[i])
            break;
        bool optParsed = false;
        while (opts->parse && !optParsed)
        {
            // Check if this needs to be an argument, not an option
            if (!opts->shortOpt && argv[i][0] != '-')
            {
                optParsed = true;
                // This matches. Parse it
                if (!opts->parse (opts, argv[i]))
                    return false;
            }
            // Was the short option specified?
            else if (argv[i][0] == '-' && argv[i][1] != '-' &&
                     argv[i][1] == opts->shortOpt)
            {
                optParsed = true;
                if (!parseArg (argv, &i, opts))
                    return false;
            }
            // Was a long option specified
            else if (argv[i][0] == '-' && argv[i][1] == '-' &&
                     !strcmp (argv[i] + 2, opts->longOpt))
            {
                optParsed = true;
                if (!parseArg (argv, &i, opts))
                    return false;
            }
            ++opts;
        }
        if (!optParsed)
        {
            error ("unrecognized option \"%s\"", argv[i]);
            return false;
        }
    }
    return true;
}

// Parses arguments
// It does this by getting the action, and then it registers an argument table for
// the action After that, it executes whatever the argument table tells it to
bool parseArgs (int argc, char** argv)
{
    // argv[1] must contain the action
    // Ensure it does
    if (!argv[1])
    {
        error (_ ("action not specified"));
        return false;
    }
    if (argv[1][0] == '-')
    {
        error (_ ("first argument must be action"));
        return false;
    }
    const char* actionName = argv[1];

    // Exceptions: check if action is "help" or "version", and if so, handle those
    // specially
    if (!strcmp (actionName, "help"))
    {
        printf (_ ("\
%s - an efficient, user-friendly package manager\n\
Usage: %s action [options]\n\
\n\
nnpkg is the CLI frontend to the nnpkg infastructure.\n\
It provides commands to install, remove, search, and perform other operations\n\
on packages. For more info on nnpkg in general, see nnpkg(8).\n\
Here is a list of supported actions:\n\
\n\
  add - adds specified package. Package must already have been unpacked into\n\
        filesystem\n\
  remove - removes specified package from database, and cleans up its files\n\
  init - initializes a new package database\n\
\n\
For more info on these actions, look at the man page for the action.\n\
Said man page is in the form nnpkg-ACTION(1).\n\
For info on configuring nnpkg, see nnpkg.conf(5)\n"),
                getprogname(),
                getprogname());
        exit (0);
    }
    else if (!strcmp (actionName, "version"))
    {
        printf ("nnpkg version %s\n\
nnpkg is open source software, licensed under the Apache 2.0 License.\n\
Please consult source to review the full license.\n",
                NNPKG_VERSION);
        exit (0);
    }

    // We now have the action. Find action table entry
    size_t numActions = ARRAY_SIZE (actions);
    for (int i = 0; i < numActions; ++i)
    {
        if (!strcmp (actions[i].name, actionName))
        {
            // Parse action arguments, skipping over action and argv[0]
            action = &actions[i];
            return parseAction (argc - 2, argv + 2);
        }
    }
    error (_ ("\"%s\" is not a valid action"), actionName);
    return false;
}

int main (int argc, char** argv)
{
    setprogname (argv[0]);
#ifdef NNPKG_ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain ("nnpkg", NNPKG_LOCALE_BASE);
#endif
    // Parse arguments
    if (!parseArgs (argc, argv))
        return 1;
    // Run action
    if (!action->run())
        return 1;
    return 0;
}
