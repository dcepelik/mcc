#include "error.h"
#include <stdlib.h>


const char *error_str(mcc_error_t err)
{
	switch (err) {
	case MCC_ERROR_ACCESS:
		return "Access denied.";

	case MCC_ERROR_EOF:
		return "(This is bollocks.)"; // TODO

	case MCC_ERROR_OK:
		return "No error.";

	case MCC_ERROR_NOENT:
		return "File not found.";
	}

	return NULL;
}
