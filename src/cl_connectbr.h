/*
Copyright (C) 2024 unezQuake team
cl_connectbr.h - Smart route selection for connectbr/connectnext commands
*/
#ifndef __CL_CONNECTBR_H__
#define __CL_CONNECTBR_H__

// quakedef.h must be included before this header (provides cvar_t etc.)
// cl_main.c already includes quakedef.h before cl_connectbr.h so this is fine.
// If included standalone, add: #include "quakedef.h"

// CVars (defined in cl_connectbr.c — include quakedef.h before this header if needed)

// Public functions
void CL_ConnectBR_Init(void);          // register cvars — call from CL_InitLocal
void CL_ConnectBR_Frame(void);         // call every frame from main loop — call from CL_Frame
void CL_Connect_BestRoute_f(void);     // connectbr command
void CL_Connect_Next_f(void);          // connectnext command
void CL_Connect_Previous_f(void);      // connectprevious command
void CL_Connect_Info_f(void);          // connectinfo command

#endif // __CL_CONNECTBR_H__
