#define	NSUMS	10
#define	NPOINT	100
#define	BUFFSZ	1000

#define	INVIS		1l <<  0 /* invisible parameter (Checkin)      	*/
#define	DATE		1l <<  1 /* DATE parameter mm-dd-yy (Checkout)	*/
#define	AMOUNT		1l <<  2 /* dollar AMOUNT parameter (Checkout)	*/
#define	AUTO		1l <<  3 /* AUTOmatic insertion (Checkin)	*/
#define	NUMB		1l <<  4 /* integer NUMBer (Checkout)		*/
#define	NDECPT		1l <<  5 /* number w/wo decimal point (Checkout)*/
#define	SKIP		1l <<  6 /* SKIP field (Checkin)		*/
#define	DAYST		1l <<  7 /* DAY STtring like mwf (Checkout)	*/
#define	TIMEST		1l <<  8 /* TIME STring ie 1:00-2:00 (Checkout)	*/
#define	NOTNUL		1l <<  9 /* give error if field null (Checkout)	*/
#define	DICT		1l << 10 /* DICTionary verification (Checkout)	*/
#define	TOOLONG		1l << 11 /* check field length (Checkout)	*/
#define	KEYTHERE	1l << 12 /* this key must be present (Checkout)	*/
#define	UCN		1l << 13 /* this is a UC Number (Checkout)	*/
#define	TOOSHORT	1l << 14 /* field is too short (Checkout)	*/
#define	END		1l << 15 /* last item in format.h file         	*/
#define	DATE2		1l << 16 /* DATE parameter yy-mm-dd (Checkout)	*/
#define	DATE1   	1l << 17 /* DATE parameter mm-yy (Checkout)	*/
#define	POSITIVE	1l << 18 /* must be a POSITIVE number (Checkout)*/
#define	UNIQUE		1l << 19 /* field identifies record (Merge)	*/
#define	ALENGTH		1l << 20 /* No length checking (Letter)		*/
#define	MULT		1l << 21 /* Allow multiple line entries		*/
