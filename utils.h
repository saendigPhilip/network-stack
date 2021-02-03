//
// Created by philip on 02.02.21.
//
#include <librpma.h>

#ifndef NETWORK_STACK_UTILS_H
#define NETWORK_STACK_UTILS_H

extern const char* usage_string;
extern char* hostname;
extern char* port;

extern struct rpma_peer* peer;

/**
 * Parses the arguments. If arguments are invalid, either -1 is returned or the user
 * is asked whether to choose default values
 * Sets the variables hostname and port
 *
 * @param hostname      Location where the hostname will be stored
 * @param port          Int-Pointer to where the port will be stored
 * @return              0  on success
 *                      -1 if parsing the arguments failed
 */
int parseargs(int argc, char** argv);

/**
 * Initializes the rpma_peer structure that is needed by both, client and server to
 * communicate via RPMA
 *
 * @return  0  on success
 *          -1 if some error occurs and the peer structure can't be initialized
 */
int initialize_peer();

#endif //NETWORK_STACK_UTILS_H
