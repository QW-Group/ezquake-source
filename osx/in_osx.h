//_________________________________________________________________________________________________________________________nFO
// "in_osx.h" - MacOS X mouse driver
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//_____________________________________________________________________________________________________________________dEFINES

#pragma mark =Defines=

#define	IN_MOUSE_BUTTONS	5

#pragma mark -

//____________________________________________________________________________________________________________________tYPEDEFS

#pragma mark =TypeDefs=

typedef struct	{
                        SInt32		X;
                        SInt32		Y;
                        SInt32		OldX;
                        SInt32		OldY;
                }	in_mousepos_t;

#pragma mark -

//___________________________________________________________________________________________________________________vARIABLES

#pragma mark =Variables=

extern BOOL			gInMouseEnabled;
extern UInt8		gInSpecialKey[];
extern UInt8		gInNumPadKey[];

#pragma mark -

//_________________________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

void	IN_CenterCursor (void);
void	IN_ShowCursor (BOOL);
void 	IN_InitMouse (void);
void 	IN_ReceiveMouseMove (CGMouseDelta, CGMouseDelta);
void 	IN_MouseMove (usercmd_t *);
void	IN_SetF12EjectEnabled (qbool theState);

//_________________________________________________________________________________________________________________________eOF
