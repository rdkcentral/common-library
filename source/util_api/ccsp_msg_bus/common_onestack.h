#ifdef _ONESTACK_PRODUCT_REQ_

bool is_bci_partner(void);

/* Business Gateway - runtime partner check */
#define BCI_PRODUCT_CHECK if (is_bci_partner()) \
     {

#define BCI_PRODUCT_CHECK_END }

/* Residential Gateway - runtime partner check (inverse) */
#define RES_PRODUCT_CHECK if (!is_bci_partner()) \
     {

#define RES_PRODUCT_CHECK_END }

#else
#define BCI_PRODUCT_CHECK
#define BCI_PRODUCT_CHECK_END
#define RES_PRODUCT_CHECK
#define RES_PRODUCT_CHECK_END
#endif
