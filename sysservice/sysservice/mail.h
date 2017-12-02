#ifndef __MAIL_H_
#define __MAIL_H__

#ifdef __cplusplus 
extern "C" {
#endif
    // In computer science, an opaque data type is a data type that is incompletely defined in an interface, 
	// so that its values can only be manipulated by calling subroutines that have access to the missing information. 
	// The concrete representation of the type is hidden from its users. A data type whose representation is visible is
	// called transparent.
	struct mail;

	/* 成功返回mail，失败返回 NULL */
	struct mail *newmail(void);
    void deletemail(struct mail * __restrict * __restrict mail);
	/* 以 NULL 表示可变参数的结束，并且可变参数是附件文件列表，文件名类型是 const wchar_t * 。
	  成功返回0，失败返回 -1	*/
    int makemail(struct mail *__restrict mail,
	    const unsigned char *__restrict from, 
	    const unsigned char *__restrict to, 
	    const unsigned char *__restrict subject, 
	    const unsigned char *__restrict smtpserver,
	    const unsigned char *__restrict username,
	    const unsigned char *__restrict password,
	    const unsigned char *__restrict content, ...);
	/* 失败返回0，成功返回新增加的字节数 */
    int unsigned catmail(struct mail * __restrict mail, const unsigned char * __restrict format, ...);
	/* 以 NULL 表示可变参数的结束，并且可变参数是附件文件列表，文件名类型是 const wchar_t * 。
	  成功返回0，失败返回 -1	*/
    int makemailw(struct mail *__restrict mail,
	    const wchar_t *__restrict from, 
	    const wchar_t *__restrict to, 
	    const wchar_t *__restrict subject, 
	    const wchar_t *__restrict smtpserver,
	    const wchar_t *__restrict username,
	    const wchar_t *__restrict password,
	    const wchar_t *__restrict content, ...);
	/* 失败返回0，成功返回新增加的字节数 */
    int unsigned catmailw(struct mail * __restrict mail, const wchar_t * __restrict format, ...);

	/* 失败返回0，成功返回新增加的字节数 */
    int unsigned catmailattachment(struct mail * __restrict mail, const wchar_t *attachmentfile);
	void completemail(struct mail * __restrict mail);
	/* 成功返回0，失败返回 -1 */
	int  incmailsize(struct mail * __restrict mail, unsigned int incsizebytes);
	/* 成功返回0，失败返回 -1 */
	int sendmail(const struct mail * __restrict mail);

#ifdef __cplusplus 
}
#endif

#endif
