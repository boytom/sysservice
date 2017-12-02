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

	/* �ɹ�����mail��ʧ�ܷ��� NULL */
	struct mail *newmail(void);
    void deletemail(struct mail * __restrict * __restrict mail);
	/* �� NULL ��ʾ�ɱ�����Ľ��������ҿɱ�����Ǹ����ļ��б��ļ��������� const wchar_t * ��
	  �ɹ�����0��ʧ�ܷ��� -1	*/
    int makemail(struct mail *__restrict mail,
	    const unsigned char *__restrict from, 
	    const unsigned char *__restrict to, 
	    const unsigned char *__restrict subject, 
	    const unsigned char *__restrict smtpserver,
	    const unsigned char *__restrict username,
	    const unsigned char *__restrict password,
	    const unsigned char *__restrict content, ...);
	/* ʧ�ܷ���0���ɹ����������ӵ��ֽ��� */
    int unsigned catmail(struct mail * __restrict mail, const unsigned char * __restrict format, ...);
	/* �� NULL ��ʾ�ɱ�����Ľ��������ҿɱ�����Ǹ����ļ��б��ļ��������� const wchar_t * ��
	  �ɹ�����0��ʧ�ܷ��� -1	*/
    int makemailw(struct mail *__restrict mail,
	    const wchar_t *__restrict from, 
	    const wchar_t *__restrict to, 
	    const wchar_t *__restrict subject, 
	    const wchar_t *__restrict smtpserver,
	    const wchar_t *__restrict username,
	    const wchar_t *__restrict password,
	    const wchar_t *__restrict content, ...);
	/* ʧ�ܷ���0���ɹ����������ӵ��ֽ��� */
    int unsigned catmailw(struct mail * __restrict mail, const wchar_t * __restrict format, ...);

	/* ʧ�ܷ���0���ɹ����������ӵ��ֽ��� */
    int unsigned catmailattachment(struct mail * __restrict mail, const wchar_t *attachmentfile);
	void completemail(struct mail * __restrict mail);
	/* �ɹ�����0��ʧ�ܷ��� -1 */
	int  incmailsize(struct mail * __restrict mail, unsigned int incsizebytes);
	/* �ɹ�����0��ʧ�ܷ��� -1 */
	int sendmail(const struct mail * __restrict mail);

#ifdef __cplusplus 
}
#endif

#endif
