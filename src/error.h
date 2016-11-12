#ifndef ERROR_H
#define ERROR_H

enum mcc_error {
	MCC_ERROR_ACCESS,
	MCC_ERROR_EOF,
	MCC_ERROR_NOMEM,
	MCC_ERROR_OK,
};

typedef enum mcc_error mcc_error_t;

#endif
