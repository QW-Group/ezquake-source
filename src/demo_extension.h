#ifndef EZQUAKE_DEMO_EXTENSION_H
#define EZQUAKE_DEMO_EXTENSION_H

void DemoExtension_Init(void);
void DemoExtension_UpdateFrame(void);
void DemoExtension_StartDemo(void);
void DemoExtension_StopDemo(void);
qbool DemoExtension_Load(struct vfsfile_s *file, const char *name);
qbool DemoExtension_LoadFile(const char *path, byte **data, int *len);

#endif
