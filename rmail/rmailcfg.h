#define		MIMEPARTHEADER		/* mime part headers if 8bit-header=no */
#undef		CHANGEPARTHEADER	/* add Content-Type & Content-Transfer-Encoding to part headers */
#undef		SAVECHARSET	 		/* save unknown charset when unmime header */
#define		UNFOLDINGSUBJ		/* make 1-line subject when local delivery */
#undef		CHANGETRANSIT		/* mime and xlat transit messages */

#define MAXHOPS	 	30
#define MAXUNFOLD	200
