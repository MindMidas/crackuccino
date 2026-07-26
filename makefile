# compiler & flags
CC := gcc
MPICC := mpicc
CFLAGS := -O2 -Wall -Wextra -std=c11 -Isrc

# shared file
SHARED := src/shared.c

# targets
ENCRYPT_SRCS := src/encrypt.c $(SHARED)
SDECRYPT_SRCS := src/decrypt_serial.c src/hash_dict.c src/hit_buffer.c src/time_util.c $(SHARED)
MDECRYPT_SRCS := src/decrypt_mpi.c src/decrypt_serial.c src/hash_dict.c src/hit_buffer.c src/time_util.c $(SHARED)
KEY_DECRYPT_SRCS := src/decrypt_key.c src/decrypt_serial.c src/hash_dict.c src/hit_buffer.c src/time_util.c $(SHARED)

# build
.PHONY: all clean

all: clean encrypt decrypt-serial decrypt-mpi decrypt-key

# encrypt app main
encrypt:
	$(CC) $(CFLAGS) -o $@ $(ENCRYPT_SRCS)

# serial app main
decrypt-serial:
	$(CC) $(CFLAGS) -DBUILD_SERIAL_APP -o $@ $(SDECRYPT_SRCS)

# MPI app main
decrypt-mpi:
	$(MPICC) $(CFLAGS) -o $@ $(MDECRYPT_SRCS)

# direct known-key decrypt app main
decrypt-key:
	$(CC) $(CFLAGS) -o $@ $(KEY_DECRYPT_SRCS)

# cleaning
clean:
	rm -f encrypt decrypt-serial decrypt-mpi decrypt-key ciphertext.txt
