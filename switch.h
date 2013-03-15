#ifndef SWITCH_H
#define SWITCH_H

#define PTHREAD_NUMBER	1	/*线程越大，越容易出现失败的帐号，所以，建议第一次线程为10，然后以2线程运行第二次*/
//#define SKIP_PEER_VERIFICATION
//#define SKIP_HOSTNAME_VERFICATION

#define USE_OPENSSL
//The Define Macro for function switch

#define HTTPS_DEBUG				/*用于调试*/
//#define S_ADD_TO_CART				/*添加到购物车*/
//#define S_EMPTY_CART				/*清空购物车*/
//#define S_10_DIAN					/*10点抢购功能*/
//#define S_MAKE_WISH				/*许愿功能*/
//#define S_MI_4_REGISTER			/*抢购注册功能，注意：使用该功能一定要将线程设置为1，避免对变量tel的同时访问，造成dirty data*/
//#define S_PANIC_BUYING				/*抢购功能*/
//#define S_PANIC_BUYING_USE_AUTH	/*抢购是否需要验证*/
//#define S_PANIC_BUYING_AUTOREFERER	/*抢购函数设置自动refer*/

//End of the function switch

#define PANIC_FORM	"version=1"

#endif
