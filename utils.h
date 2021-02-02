//
// Created by philip on 02.02.21.
//

#ifndef NETWORK_STACK_UTILS_H
#define NETWORK_STACK_UTILS_H

extern const char* usage_string;
extern char* hostname;
extern char* port;

/**
 * Parses the arguments. If arguments are invalid, either -1 is returned or the user
 * is asked whether to choose default values
 * Sets the variables hostname and port
 *
 * @param hostname      Location where the hostname will be stored
 * @param port          Int-Pointer to where the port will be stored
 * @return              0  on success
 *                      -1 if parsing the arguments failed
 *                      -2 if any parameter is NULL
 */
int parseargs(int argc, char** argv);

#endif //NETWORK_STACK_UTILS_H
