#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "shared.h"

void build_encrypt_dict(const char *input_dict, char *encrypt_dict);
int set_custom_encrypt_dict(const char *input_dict, const char *custom_dict, char *encrypt_dict);
void encrypt_plaintext(const char *plaintext, const char *input_dict, const char *encrypt_dict, char *ciphertext);
void write_ciphertext(const char *ciphertext);


#endif
