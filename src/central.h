
#ifndef CENTRAL_H
#define CENTRAL_H

#include "server.h"

void Central_Init(void);
void Central_Shutdown(void);
void Central_ProcessResponses(void);
// void Central_SubmitGame(const char* path);

// Creates a challenge/response on the web server after user claims to be 'username'
void Central_GenerateChallenge(client_t* client, const char* username, qbool during_login);

// Checks with the server if a client's response to a challenge is correct
void Central_VerifyChallengeResponse(client_t* client, const char* challenge, const char* response);

#ifndef SERVER_ONLY
// 
void Central_FindChallengeResponse(const char* token, const char* challengeText);
#endif

#endif // !CENTRAL_H
