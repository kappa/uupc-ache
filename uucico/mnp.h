/* Declarations from MNP emulation interface */

extern int Mx5done;

#define mx5_present() (Mx5done != -1 ? (boolean)Mx5done : mnp_present())

boolean mnp_present(void);
void  set_mnp_level(int  level);
int  get_mnp_level(void);
void  set_mnp_wait_tics(int tics);
int get_mnp_wait_tics(void);
boolean mnp_active(void);
int  mnp_level(void);
void wait_tics(unsigned tics);
void set_answer_mode(boolean mode);
void set_sound(boolean mode);
void MnpSupport(void);

