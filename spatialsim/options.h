#ifndef OPTIONS_H_
#define OPTIONS_H_

#include "mystruct.h"
#include "sbml/SBMLTypes.h"

optionList getOptionList(int argc, char **argv, SBMLDocument *doc);
void printErrorMessage();

#endif