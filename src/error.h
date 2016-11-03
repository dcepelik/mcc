#ifndef ERROR_H
#define ERROR_H

enum mcc_error {
	MCC_ERROR_OK,
	MCC_ERROR_NOMEM,
	MCC_ERROR_ACCESS,
};

typedef enum mcc_error mcc_error_t;

#endif
