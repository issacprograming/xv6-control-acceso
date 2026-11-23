#ifndef XV6_AUTH_H
#define XV6_AUTH_H

#define AUTH_NAME_MAX 16
#define AUTH_PASSWORD_MAX 64
#define STARTSESSION_DENIED (-2)

enum login_state { LOGIN_NONE, LOGIN_PENDING, LOGIN_VERIFIED };

#endif
