#ifndef FILEIO_HTTP_H
#define FILEIO_HTTP_H

int     SocketIO_Open     ( const char* hostname, unsigned short port );
size_t  SocketIO_Send     ( int socket, const void*  ptr, size_t len );
size_t  SocketIO_Receive  ( int socket, void*        ptr, size_t len );
int     SocketIO_Close    ( int socket );

int     HTTP_open         ( const char* URL );
int     HTTP_close        ( int socket );

/*
int  recv       ( int socket, void*       buf, size_t len, int flags );
int  recvfrom   ( int socket, void*       buf, size_t len, int flags, struct sockaddr*     from, socklen_t* from_len );
int  recvmsg    ( int socket, struct msghdr*       msg, int flags );

int  send       ( int socket, const void* msg, size_t len, int flags );
int  sendto     ( int socket, const void* msg, size_t len, int flags, const struct sockaddr* to, socklen_t to_len );
int  sendmsg    ( int socket, const struct msghdr* msg, int flags );
*/

#endif /* FILEIO_HTTP_H */
