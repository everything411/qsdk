/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Userspace OP-TEE Client Application for FuseIPQ functionality
 *
 * This application provides a userspace interface to blow fuses using
 * the fuseipq Trusted Application (TA) running in OP-TEE secure world.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <tee_client_api.h>

/*
 * TA Interface Definitions
 * These will need to be updated once the actual fuseipq TA is implemented
 */

#define FUSEIPQ_TA_UUID \
    { 0x7e5e8c5d, 0x5375, 0x4ab0, \
      { 0x8b, 0x11, 0x95, 0x14, 0x96, 0x65, 0xdc, 0x88 } }

/* Command IDs for TA operations */
#define CMD_BLOW_FUSE                     0x04

/* Fuse provisioning status codes */
#define FUSEPROV_SUCCESS                  0x00
#define FUSEPROV_INVALID_HASH             0x01
#define IMAGE_AUTH_FAILURE                0x02
#define FUSEPROV_ERROR_GENERIC            0xFF

/*
 * Global OP-TEE context and session
 */
static TEEC_Context ctx;
static TEEC_Session sess;
static int session_opened = 0;

/*
 * Initialize OP-TEE context and open session with fuseipq TA
 */
static int fuseipq_init(void)
{
    TEEC_Result res;
    TEEC_UUID uuid = FUSEIPQ_TA_UUID;
    uint32_t err_origin;

    /* Initialize TEE context */
    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS) {
        fprintf(stderr, "TEEC_InitializeContext failed with code 0x%x\n", res);
        return -1;
    }

    /* Open session with TA */
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                          TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS) {
        fprintf(stderr, "TEEC_OpenSession failed with code 0x%x origin 0x%x\n",
                res, err_origin);
        TEEC_FinalizeContext(&ctx);
        return -1;
    }

    session_opened = 1;
    printf("Successfully opened session with fuseipq TA\n");
    return 0;
}

/*
 * Cleanup OP-TEE context and session
 */
static void fuseipq_cleanup(void)
{
    if (session_opened) {
        TEEC_CloseSession(&sess);
        session_opened = 0;
    }
    TEEC_FinalizeContext(&ctx);
}

/*
 * Read file contents into a buffer
 * Returns: file size on success, -1 on error
 */
static ssize_t read_file(const char *path, void **buffer)
{
    int fd;
    struct stat st;
    ssize_t size, bytes_read;
    void *buf;

    /* Open file */
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file %s: %s\n", path, strerror(errno));
        return -1;
    }

    /* Get file size */
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "Failed to stat file %s: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }
    size = st.st_size;

    if (size <= 0) {
        fprintf(stderr, "Invalid file size: %zu\n", size);
        close(fd);
        return -1;
    }

    /* Allocate buffer */
    buf = malloc(size);
    if (!buf) {
        fprintf(stderr, "Failed to allocate %zu bytes\n", size);
        close(fd);
        return -1;
    }

    /* Read file contents */
    bytes_read = read(fd, buf, size);
    if (bytes_read != size) {
        fprintf(stderr, "Failed to read file: expected %zu, got %zu\n",
                size, bytes_read);
        free(buf);
        close(fd);
        return -1;
    }

    close(fd);
    *buffer = buf;
    return size;
}

/*
 * Blow fuses using the provided sec.elf file
 * This function:
 * 1. Reads the sec.elf file
 * 2. Allocates shared memory
 * 3. Invokes the TA to authenticate and blow fuses
 * 4. Returns the fuse status
 */
static int fuseipq_blow_fuse(const char *sec_elf_path)
{
    TEEC_Result res;
    TEEC_Operation op;
    TEEC_SharedMemory shm;
    uint32_t err_origin;
    void *file_buf = NULL;
    ssize_t file_size;
    int ret = -1;
    uint32_t fuse_status;

    printf("Reading sec.elf file: %s\n", sec_elf_path);

    /* Read sec.elf file */
    file_size = read_file(sec_elf_path, &file_buf);
    if (file_size < 0) {
        return -1;
    }

    printf("File size: %zu bytes\n", file_size);

    /* Allocate shared memory for the file content */
    shm.size = file_size;
    shm.flags = TEEC_MEM_INPUT;

    res = TEEC_AllocateSharedMemory(&ctx, &shm);
    if (res != TEEC_SUCCESS) {
        fprintf(stderr, "TEEC_AllocateSharedMemory failed with code 0x%x\n", res);
        free(file_buf);
        return -1;
    }

    /* Copy file content to shared memory */
    memcpy(shm.buffer, file_buf, file_size);
    free(file_buf);

    /* Prepare operation parameters */
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
                                     TEEC_VALUE_OUTPUT,
                                     TEEC_NONE,
                                     TEEC_NONE);

    /* param[0]: Input buffer containing sec.elf content */
    op.params[0].memref.parent = &shm;
    op.params[0].memref.offset = 0;
    op.params[0].memref.size = file_size;

    /* param[1]: Output value for fuse status */
    op.params[1].value.a = 0;

    printf("Invoking TA to blow fuses...\n");

    /* Invoke TA command */
    res = TEEC_InvokeCommand(&sess, CMD_BLOW_FUSE, &op, &err_origin);

    if (res != TEEC_SUCCESS) {
        fprintf(stderr, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n",
                res, err_origin);
        goto cleanup;
    }

    /* Get fuse status from TA */
    fuse_status = op.params[1].value.a;

    /* Interpret fuse status */
    switch (fuse_status) {
    case FUSEPROV_SUCCESS:
        printf("Fuse blow SUCCESS\n");
        ret = 0;
        break;
    case FUSEPROV_INVALID_HASH:
        fprintf(stderr, "Fuse blow FAILED: Invalid sec.elf (hash mismatch)\n");
        ret = -1;
        break;
    case IMAGE_AUTH_FAILURE:
        fprintf(stderr, "Fuse blow FAILED: Image authentication failed\n");
        ret = -1;
        break;
    default:
        fprintf(stderr, "Fuse blow FAILED: Unknown error code 0x%x\n", fuse_status);
        ret = -1;
        break;
    }

cleanup:
    TEEC_ReleaseSharedMemory(&shm);
    return ret;
}

/*
 * Print usage information
 */
static void print_usage(const char *prog_name)
{
    printf("Usage: %s <sec.elf path>\n", prog_name);
    printf("\n");
    printf("Blow fuses using the provided sec.elf file via OP-TEE fuseipq TA\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  <sec.elf path>  Path to the sec.elf file for fuse provisioning\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s /tmp/sec.elf\n", prog_name);
}

/*
 * Main entry point
 */
int main(int argc, char *argv[])
{
    int ret;

    printf("FuseIPQ Client Application v1.0\n");
    printf("================================\n\n");

    /* Check arguments */
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    /* Initialize OP-TEE context and session */
    if (fuseipq_init() != 0) {
        fprintf(stderr, "Failed to initialize fuseipq CA\n");
        return 1;
    }

    /* Perform fuse blow operation */
    ret = fuseipq_blow_fuse(argv[1]);

    /* Cleanup */
    fuseipq_cleanup();

    if (ret == 0) {
        printf("\nFuse provisioning completed successfully\n");
        return 0;
    } else {
        printf("\nFuse provisioning FAILED\n");
        return 1;
    }
}
