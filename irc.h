/**
	\file

	\brief
	Internet Relay Chat Support

	\author
	johnnycz
**/

#ifdef WITH_IRC
/// initializes the IRC module, creates libircclient session 
void IRC_Init(void);

/// process new data waiting on the socket
void IRC_Update(void);

/// return the name of the active (currently selected) channel from the channel list
char* IRC_GetCurrentChan(void);

/// select following channel from the channels list as the current channel
void IRC_NextChan(void);

/// select previous channel from the channels list as the current channel
void IRC_PrevChan(void);
#endif