#ifndef DECRYPT_MPI_H
#define DECRYPT_MPI_H

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "decrypt_serial.h"
#include "shared.h"
#include "hash_dict.h"
#include "hit_buffer.h"
#include "time_util.h"

int mpi_bcast_cstring(int root, MPI_Comm comm, char **buf_inout);

#endif