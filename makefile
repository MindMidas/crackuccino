# compiler & flags
CC := gcc
MPICC := mpicc
CFLAGS := -O2 -Wall -Wextra -std=c11 -Isrc/native

# shared file
SHARED := src/native/shared.c

# targets
ENCRYPT_SRCS := src/native/encrypt.c $(SHARED)
SDECRYPT_SRCS := src/native/decrypt_serial.c src/native/hash_dict.c src/native/hit_buffer.c src/native/time_util.c $(SHARED)
MDECRYPT_SRCS := src/native/decrypt_mpi.c src/native/decrypt_serial.c src/native/hash_dict.c src/native/hit_buffer.c src/native/time_util.c $(SHARED)

# build
all: clean encrypt decrypt-serial decrypt-mpi

# encrypt app main
encrypt:
	$(CC) $(CFLAGS) -o $@ $(ENCRYPT_SRCS)

# serial app main
decrypt-serial:
	$(CC) $(CFLAGS) -DBUILD_SERIAL_APP -o $@ $(SDECRYPT_SRCS)

# MPI app main
decrypt-mpi:
	$(MPICC) $(CFLAGS) -o $@ $(MDECRYPT_SRCS)

# cleaning
clean:
	rm -f encrypt decrypt-serial decrypt-mpi ciphertext.txt
