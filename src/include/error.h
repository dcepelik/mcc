#ifndef ERROR_H
#define ERROR_H

enum mcc_error {
	MCC_ERROR_ACCESS,
	MCC_ERROR_EOF,
	MCC_ERROR_OK,
	MCC_ERROR_NOENT,
};

typedef enum mcc_error mcc_error_t;

const char *error_str(mcc_error_t err);

#endif
