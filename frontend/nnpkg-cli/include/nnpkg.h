/*
    nnpkg.h - contains nnpkg-cli main header
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

#ifndef _NNPKG_H
#define _NNPKG_H

#include <config.h>
#include <stdbool.h>

typedef struct _actionOpt actionOption_t;

// Function type to parse an argument
typedef bool (*parseOption) (actionOption_t*, char*);

// Action argument table entry
typedef struct _actionOpt
{
    const char shortOpt;    // Short option letter
    const char* longOpt;    // Long option letter
    parseOption parse;      // Function to parse option
    bool argRequired;       // If an argument is required
} actionOption_t;

// Function type to get the argument table
typedef actionOption_t* (*getOptionTable)();

// Function type to run an action
typedef bool (*runAction)();

// Action table entry
typedef struct _action
{
    const char* name;              // Name of this action
    getOptionTable getOptTable;    // Function to get option table
    runAction run;                 // Function to run this action
} action_t;

// Action hooks
actionOption_t* initGetOptions();
bool initRunAction();

actionOption_t* addGetOptions();
bool addRunAction();

#endif
