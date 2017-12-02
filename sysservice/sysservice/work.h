#ifndef __WORK_H__
#define __WORK_H__

#ifdef __cplusplus 
extern "C" {
#endif
	void starttask(void);
	void stoptask(void);
	void pausetask(void);
	void continuetask(void);
	void printconfig(FILE *fp);
#ifdef __cplusplus 
}
#endif

#endif