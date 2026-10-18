/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "pkcs11_test.h"

void print_usage(void)
{
	printf("\nUsage of pkcs11_test_app:\n");

	printf("\nThis test app is used to call the qseecom IOCTLS"
	       "which are used for PKCS#11 and get the return status\n");

	printf("\n1. pkcs_test_app 1 <mdt_file> <seg_file>\n\n");
	printf("    TA library load ioctl will be called with the mdt and concatenated segment files\n");

	printf("\n2. pkcs_test_app 2 <mdt_file> <seg_file>\n\n");
	printf("    TA App load ioctl will be called with the mdt and concatenated segment files\n");

	printf("\n3. pkcs_test_app 3 <ecdsa_blob>\n\n");
	printf("    ECDSA Import key ioctl will be called with the given blob. Key_handle will be printed\n");

	printf("\n4. pkcs_test_app 4 <key_handle>\n\n");
	printf("    set key_handle ioctl will be called with the given key_handle\n");

	printf("\n5. pkcs_test_app 5 <digest_file>\n\n");
	printf("    Sign ioctl will be called with the given digest_file and output signature will be stored at sig_data\n"
	       "    Max supported Digest size is 64 bytes\n");

	printf("\n6. pkcs_test_app 6 <digest_file> <sig_data>\n\n");
	printf("    Verify ioctl will be called with the given digest_file and signature file\n");

	printf("\n7. pkcs_test_app 7\n\n");
	printf("    Unload TA Library ioctl will be called.\n");

	printf("\n8. pkcs_test_app 8\n\n");
	printf("    Unload TA App ioctl will be called\n");
}

int ecdsa_import_key(const char *path)
{
	struct ecdsa_import_key key = {0};
	int fd_blob, fd_ioctl, ret;
	struct stat st;

	fd_blob = open(path, O_RDONLY);
	if (fd_blob < 0) {
		printf("Failed to open %s\n", path);
		return -EACCES;
	}

	fstat(fd_blob, &st);

	key.ecdsa_blob = malloc(st.st_size);
	key.ecdsa_blob_len = st.st_size;

	ret = read(fd_blob, key.ecdsa_blob, key.ecdsa_blob_len);
	if (ret < 0) {
		printf("Failed to read %s\n", path);
		goto free_blob;
	};

	fd_ioctl = open(qseecom_device, O_RDWR);
	if (fd_ioctl < 0) {
		printf("Cannot open device file, %s\n", qseecom_device);
		ret = -EIO;
		goto free_blob;
	}

	ret = ioctl(fd_ioctl, QSEECOM_ECDSA_IMPORT_KEY, &key);
	if (ret || key.result) {
		printf("QSEECOM_ECDSA_IMPORT_KEY failed, ret = %d, result = %d\n", ret, key.result);
		goto close_ioctl;
	}

	printf("Import key success KEY: 0x%x\n", key.key_handle);

close_ioctl:
	close(fd_ioctl);

free_blob:
	free(key.ecdsa_blob);

	close(fd_blob);

	return ret;
}

int ecdsa_set_key(unsigned int key_handle)
{
	int fd, ret;

	fd = open(qseecom_device, O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file, %s\n", qseecom_device);
		return -EIO;
	}

	ret = ioctl(fd, QSEECOM_SET_KEY_HANDLE, &key_handle);
	if (ret)
		printf("QSEECOM_SET_KEY_HANDLE failed, %d\n", ret);

	close(fd);

	return ret;
}

int load_lib(const char *file1, const char *file2)
{
	struct ta_info info;
	int fd, ret;

	strlcpy(info.mdt_file, file1, sizeof(info.mdt_file));
	strlcpy(info.seg_file, file2, sizeof(info.seg_file));

	fd = open(qseecom_device, O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file, %s\n", qseecom_device);
		return -EIO;
	}

	ret = ioctl(fd, QSEECOM_LOAD_TA_LIB, &info);
	if (ret)
		printf("QSEECOM_LOAD_TA_LIB failed, %d\n", ret);

	close(fd);

	return ret;
}

int load_ta(const char *file1, const char *file2)
{
	struct ta_info info;
	int fd, ret;

	strlcpy(info.mdt_file, file1, sizeof(info.mdt_file));
	strlcpy(info.seg_file, file2, sizeof(info.seg_file));

	fd = open(qseecom_device, O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file, %s\n", qseecom_device);
		return -EIO;
	}

	ret = ioctl(fd, QSEECOM_LOAD_TA_APP, &info);
	if (ret)
		printf("QSEECOM_LOAD_TA_LIB failed, %d\n", ret);

	close(fd);

	return ret;
}

int unload_lib(void)
{
	int fd, ret;

	fd = open(qseecom_device, O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file, %s\n", qseecom_device);
		return -EIO;
	}

	ret = ioctl(fd, QSEECOM_UNLOAD_TA_LIB);
	if (ret)
		printf("QSEECOM_UNLOAD_TA_LIB failed, %d\n", ret);

	close(fd);

	return ret;
}

int unload_ta(void)
{
	int fd, ret;

	fd = open(qseecom_device, O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file, %s\n", qseecom_device);
		return -EIO;
	}

	ret = ioctl(fd, QSEECOM_UNLOAD_TA_APP);
	if (ret)
		printf("QSEECOM_UNLOAD_TA_APP failed, %d\n", ret);

	close(fd);

	return ret;
}

int ecdsa_sign(const char *file1)
{
	int fd_data, fd_ioctl, fd_sig, ret;
	struct ecdsa_sign sign = {0};
	struct stat st;

	fd_data = open(file1, O_RDONLY);
	if (fd_data < 0) {
		printf("Failed to open %s\n", file1);
		return -EACCES;
	}

	fstat(fd_data, &st);

	if (st.st_size > MAX_DIGEST_SIZE) {
		printf("Digest size exceeds MAX size %d\n", MAX_DIGEST_SIZE);
		ret = -EINVAL;
		goto close_data;
	}

	sign.data = malloc(st.st_size);
	sign.data_len = st.st_size;
	sign.signature = malloc(132);

	ret = read(fd_data, sign.data, sign.data_len);
	if (ret < 0) {
		printf("Failed to read %s\n", file1);
		goto free_data;
	};

	fd_ioctl = open(qseecom_device, O_RDWR);
	if (fd_ioctl < 0) {
		printf("Cannot open device file, %s\n", qseecom_device);
		ret = -EIO;
		goto free_data;
	}

	ret = ioctl(fd_ioctl, QSEECOM_ECDSA_SIGN, &sign);
	if (ret || sign.result) {
		printf("QSEECOM_ECDSA_SIGN failed, ret = %d, result = %d\n", ret, sign.result);
		goto close_ioctl;
	}

	fd_sig = open("sig_data", O_CREAT | O_RDWR);
	if (fd_sig < 0) {
		printf("Failed to create sig_data\n");
		goto close_ioctl;
	}

	ret = write(fd_sig, sign.signature, sign.signature_out_len);
	if (ret < 0) {
		printf("Write to sig_data failed %d\n", ret);
		goto close_sig;
	}

	ret = 0;

	printf("Signature at sig_data and length %d\n", sign.signature_out_len);

close_sig:
	close(fd_sig);
close_ioctl:
	close(fd_ioctl);

free_data:
	free(sign.signature);
	free(sign.data);

close_data:
	close(fd_data);

	return ret;
}

int ecdsa_verify(const char *file1, const char *file2)
{
	int fd_data, fd_ioctl, fd_sig, ret;
	struct ecdsa_verify verify = {0};
	struct stat st;

	fd_data = open(file1, O_RDONLY);
	if (fd_data < 0) {
		printf("Failed to open %s\n", file1);
		return -EACCES;
	}

	fstat(fd_data, &st);

	if (st.st_size > MAX_DIGEST_SIZE) {
		printf("Digest size exceeds MAX size %d\n", MAX_DIGEST_SIZE);
		ret = -EINVAL;
		goto close_data;
	}

	verify.data = malloc(st.st_size);
	verify.data_len = st.st_size;

	ret = read(fd_data, verify.data, verify.data_len);
	if (ret < 0) {
		printf("Failed to read %s\n", file1);
		goto free_data;
	};

	fd_sig = open(file2, O_RDONLY);
	if (fd_sig < 0) {
		printf("Failed to open %s\n", file2);
		goto free_data;
	}

	fstat(fd_sig, &st);

	verify.signature = malloc(st.st_size);
	verify.signature_len = st.st_size;

	ret = read(fd_sig, verify.signature, verify.signature_len);
	if (ret < 0) {
		printf("Failed to read %s\n", file2);
		goto free_sig;
	};

	fd_ioctl = open(qseecom_device, O_RDWR);
	if (fd_ioctl < 0) {
		printf("Cannot open device file, %s\n", qseecom_device);
		ret = -EIO;
		goto free_sig;
	}

	ret = ioctl(fd_ioctl, QSEECOM_ECDSA_VERIFY, &verify);
	if (ret || verify.result) {
		printf("QSEECOM_ECDSA_VERIFY failed, ret = %d, result = %d\n", ret, verify.result);
		goto close_ioctl;
	}

	printf("Verified successfully, result %d\n", verify.result);

close_ioctl:
	close(fd_ioctl);

free_sig:
	free(verify.signature);
	close(fd_sig);
free_data:
	free(verify.data);
close_data:
	close(fd_data);

	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	unsigned int key_handle;

	if (argc == 1) {
		printf("APP: Invalid input, Please check the below usage,\n");
		print_usage();
		return -1;
	}

	switch (*argv[1]) {
	case '1':
		if (argc >= 4) {
			printf("Loading TA Lib..\n");
			ret = load_lib(argv[2], argv[3]);
			if (ret)
				printf("Loading lib failed\n");
		} else
			printf("Invalid argument\n");
		break;
	case '2':
		if (argc >= 4) {
			printf("Loading TA App..\n");
			ret = load_ta(argv[2], argv[3]);
			if (ret)
				printf("Loading TA App failed\n");
		} else
			printf("Invalid argument\n");

		break;
	case '3':
		printf("Importing ECDSA Key..\n");
		ret = ecdsa_import_key(argv[2]);
		if (ret)
			printf("ecdsa_import_key failed\n");
		break;
	case '4':
		if (argc >= 3) {
			printf("Set ECDSA key_handle..\n");
			key_handle = strtoul(argv[2], NULL, 0);
			ret = ecdsa_set_key(key_handle);
			if (ret)
				printf("ecdsa_set_key failed\n");
		} else
			printf("Invalid argument\n");
		break;
	case '5':
		if (argc >= 3) {
			printf("ECDSA Signing..\n");
			ret = ecdsa_sign(argv[2]);
			if (ret)
				printf("ECDSA Sign failed\n");
		} else
			printf("Invalid argument\n");
		break;
	case '6':
		if (argc >= 4) {
			printf("ECDSA Verify..\n");
			ret = ecdsa_verify(argv[2], argv[3]);
			if (ret)
				printf("ECDSA Verify failed\n");
		} else
			printf("Invalid argument\n");
		break;
	case '7':
		printf("Unloading TA Lib..\n");
		ret = unload_lib();
		if (ret)
			printf("Unload TA failed\n");
		break;
	case '8':
		printf("Unloading TA App..\n");
		ret = unload_ta();
		if (ret)
			printf("Unload TA failed\n");
		break;
	default:
		printf("PCKS_TEST_APP: Invalid input\nPlease check the below usage\n");
		print_usage();
		return -1;
	}
	printf("pkcs_test_app return code:%d\n", ret);

	return 0;
}
