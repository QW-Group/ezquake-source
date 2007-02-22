/**
	
	HUD Editor module

	made by jogihoogi, Feb 2007
	last edit:
	$Id: hud_editor.c,v 1.1 2007-02-22 23:50:18 johnnycz Exp $

*/

#include "quakedef.h"
#include "hud.h"
extern hud_t *hud_huds;
hud_t	*last_moved_hud;
hud_t	*selected_hud;
int win_x,win_y;
vec3_t	points[3];
vec3_t	corners[4];
vec3_t	center;
vec3_t	pointer;

// selected
// 1 - resize
// 2 - move
int	selected;

cvar_t	hud_editor	=	{"hud_editor","0"};

// mouse stuff
double cursor_loc[2];

static void HUD_Editor_Draw_Tooltip(int x,int y, char *string,float r,float g,float b) {
	if (x + strlen(string)*8 > vid.width)
		x -= strlen(string)*8 ;
	if (y + 9 > vid.height)
		y -= 9;
	Draw_FillRGB(x,y,strlen(string)*8,8,r,g,b);
	Draw_String(x,y,string);
}

void mvd_vectoangles(vec3_t vec, vec3_t ang) {
	float forward, yaw, pitch;
	

		yaw = (atan2(vec[1], vec[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (vec[0]*vec[0] + vec[1]*vec[1]);
		pitch =(atan2(vec[2], forward) * 180 / M_PI);

	ang[0] = pitch;
	ang[1] = yaw;
	ang[2] = 0;
}

static int HUD_Editor_Get_Angle(int x,int y, hud_t *hud_element) {
					/*
						 2
						 90
				3	180		0	- 1
						270
						 4
				*/
	vec3_t center,pointer,angles;

	center[0] = hud_element->lx + hud_element->lw/2;
	center[1] = hud_element->ly + hud_element->lh/2;
	pointer[0] = x - center[0];
	pointer[1] = center[1] - y;

	mvd_vectoangles(pointer,angles);
	if (angles[1] < 45 || angles[1] > 315){
		return 1;
	}else if (angles[1] > 45 && angles[1] < 135){
		return 2;
	}else if (angles[1] > 135 && angles[1] < 225){
		return 3;
	}else if (angles[1] > 225 && angles[1] < 315){
		return 4;
	}
	return -1;
}

static void HUD_Editor_Move(float x,float y,float dx,float dy,hud_t *hud_element) {

	if (strcmp(hud_element->place->string,"screen")){
		Cvar_Set(hud_element->pos_x,va("%f",(hud_element->pos_x->value + dx)));
		Cvar_Set(hud_element->pos_y,va("%f",(hud_element->pos_y->value + dy)));
	}else{
		Cvar_Set(hud_element->pos_x,va("%f",x));
		Cvar_Set(hud_element->pos_y,va("%f",y));
	}

}

void HUD_Recalculate(void);

static void HUD_Editor(void){
	int i,dontmove;
	extern float mouse_x, mouse_y;
	int bx,by,bw,bh;
	char	bname[128],bplace[32],balignx[32],baligny[32];
	int found,status;
	hud_t *hud;
	hud_t *parent;
	static cvar_t	*scale;

	// gettint the huds linked list
	hud = hud_huds;

	// updating cursor location
	cursor_loc[0] += mouse_x;
	cursor_loc[1] += mouse_y;


	// check if we reached maximums
	dontmove = 0;
	for (i=0;i<2;i++){
		if (cursor_loc[i] < 0){
			cursor_loc[i] = 0;
			dontmove = 1;
		}
		if (i == 0)
			if (cursor_loc[i] > vid.width){
				cursor_loc[i] = vid.width;
				dontmove = 1;
				mouse_x = 0;
			}
		if (i == 1)
			if (cursor_loc[i] > vid.height){
				cursor_loc[i] = vid.height;
				dontmove = 1;
				mouse_y=0;
			}
	}

	// always draw the cursor
	Draw_AlphaLineRGB(cursor_loc[0],cursor_loc[1],cursor_loc[0]+2,cursor_loc[1]+2,2,0,1,0,1);

	// check if we have a hud under the cursor
	found = 0;
	while(hud->next){
		if (hud->show->value == 0){
			hud = hud->next;
			continue;
		}
		if (cursor_loc[0] >= hud->lx && cursor_loc[0] <= (hud->lx + hud->lw) && cursor_loc[1] >= hud->ly && cursor_loc[1] <= (hud->ly + hud->lh)){
			found = 1;
			break;
		}
		hud = hud->next;
	}
	// if not hud stays null
	if (!found)
		hud = NULL;

	// checks if we are moving an old hud
	if (last_moved_hud && selected_hud == 0 && !found){
		found = 1;
		hud = last_moved_hud;
	}
	
	// draw a rectangle around the currently active huds
	if(found && hud)
		Draw_OutlineRGB(hud->lx,hud->ly,hud->lw,hud->lh,0,1,0,1);
	// if we are relinking draw
	if (selected_hud)
			Draw_OutlineRGB(selected_hud->lx,selected_hud->ly,selected_hud->lw,selected_hud->lh,0,1,0,1);
	
	// check if we hit any static boxes like screen edges etc etc
	if (!found){
		if (cursor_loc[0] < 20 && cursor_loc[1] < 20){
			strcpy(bname,"screen top left");
			strcpy(bplace,"screen");
			strcpy(balignx,"left");
			strcpy(baligny,"top");
			bx=0;
			by=0;
			bw=20;
			bh=20;
			found = 1;
		}else if (cursor_loc[0] < vid.width && cursor_loc[0] > vid.width - 20 && cursor_loc[1] < 20){
			strcpy(bname,"screen top right");
			strcpy(bplace,"screen");
			strcpy(balignx,"right");
			strcpy(baligny,"top");
			by=0;
			bx=vid.width-20;
			bw=20;
			bh=20;
			found = 1;
		}else if (cursor_loc[0] < 20 && cursor_loc[1] > vid.height - 20 && cursor_loc[1] < vid.height){
			strcpy(bname,"screen bottom left");
			strcpy(bplace,"screen");
			strcpy(balignx,"left");
			strcpy(baligny,"bottom");
			bx=0;
			by= vid.height -20;
			bw=20;
			bh=20;
			found = 1;
		}else if (cursor_loc[0] < vid.width && cursor_loc[0] > vid.width -20 && cursor_loc[1] > vid.height - 20 && cursor_loc[1] < vid.height){
			strcpy(bname,"screen bottom right");
			strcpy(bplace,"screen");
			strcpy(balignx,"right");
			strcpy(baligny,"bottom");
			bx=vid.width - 20;
			by= vid.height -20;
			bw=20;
			bh=20;
			found = 1;
		}else if (cursor_loc[0] > vid.width - 20  && cursor_loc[0] < vid.width && cursor_loc[1] < vid.height - 20 && cursor_loc[1] >20){
			strcpy(bname,"screen center right");
			strcpy(bplace,"screen");
			strcpy(balignx,"right");
			strcpy(baligny,"center");
			bx=vid.width - 20;
			by=20;
			bw=20;
			bh=vid.height - 40;
			found = 1;
		}else if (cursor_loc[0] > 0  && cursor_loc[0] < 20 && cursor_loc[1] < vid.height - 20 && cursor_loc[1] > 20){
			strcpy(bname,"screen center left");
			strcpy(bplace,"screen");
			strcpy(balignx,"left");
			strcpy(baligny,"center");
			bx=0;
			by=20;
			bw=20;
			bh=vid.height - 40;
			found = 1;
		}else if (cursor_loc[0] > 20  && cursor_loc[0] < vid.width - 20 && cursor_loc[1] < vid.height && cursor_loc[1] > vid.height - 20){
			strcpy(bname,"screen bottom center");
			strcpy(bplace,"screen");
			strcpy(balignx,"center");
			strcpy(baligny,"bottom");
			bx=20;
			by=vid.height -20;
			bw=vid.width-40;
			bh=20;
			found = 1;
		}else if (cursor_loc[0] > 20  && cursor_loc[0] < vid.width - 20 && cursor_loc[1] < 20 && cursor_loc[1] > 0){
			strcpy(bname,"screen top center");
			strcpy(bplace,"screen");
			strcpy(balignx,"center");
			strcpy(baligny,"top");
			bx=20;
			by=0;
			bw=vid.width-40;
			bh=20;
			found = 1;
		}else if (cursor_loc[0] > 20  && cursor_loc[0] < 40 && cursor_loc[1] < 40 && cursor_loc[1] > 20){
			strcpy(bname,"console left");
			strcpy(bplace,"top");
			strcpy(balignx,"left");
			strcpy(baligny,"console");
			bx=20;
			by=20;
			bw=20;
			bh=20;
			found = 1;
		}else if (cursor_loc[0] > vid.width - 40  && cursor_loc[0] < vid.width -20 && cursor_loc[1] < 40 && cursor_loc[1] > 20){
			strcpy(bname,"console right");
			strcpy(bplace,"top");
			strcpy(balignx,"right");
			strcpy(baligny,"console");
			bx=vid.width-40;
			by=20;
			bw=20;
			bh=20;
			found = 1;
		}else if (cursor_loc[0] > 80  && cursor_loc[0] < vid.width -20 && cursor_loc[1] < 40 && cursor_loc[1] > 20){
			strcpy(bname,"console center");
			strcpy(bplace,"top");
			strcpy(balignx,"center");
			strcpy(baligny,"console");
			bx=40;
			by=20;
			bw=vid.width-80;
			bh=20;
			found = 1;
		}

	}
	// if the hud we are hovering above has a parent draw a line !
	if (hud)
		if (strlen(hud->place->string)){
			parent = HUD_Find(hud->place->string);
			if (parent != 0 && selected_hud == 0){
				Draw_AlphaLineRGB((hud->lx+(hud->lw/2)),hud->ly+hud->lh/2,(parent->lx+(parent->lw/2)),parent->ly+parent->lh/2,1,1,0,0,1);
			}
		}
	// draw a red line from selected hud to cursor
	if (selected_hud != 0){
		Draw_AlphaLineRGB(cursor_loc[0],cursor_loc[1],(selected_hud->lx+(selected_hud->lw/2)),selected_hud->ly+selected_hud->lh/2,1,1,0,0,1);
	}

	// if we still havent found anything we return !
	if (!found){
		if (!keydown[K_MOUSE1] && !keydown[K_MOUSE2]) {
			selected_hud = NULL;
			last_moved_hud = NULL;
		}
		return;
	}
		
	// draw the tooltips
	if (keydown[K_MOUSE1] && !keydown[K_MOUSE2] && hud){
		HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],va("%s moving",hud->name),1,0,0);
	}else if (keydown[K_MOUSE1] && keydown[K_MOUSE2] && hud){
		HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],va("%s resizing",hud->name),0,1,0);
	}else if (!keydown[K_MOUSE1] && !keydown[K_MOUSE2] && hud){
		HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],hud->name,0,0,1);
	}else if(!keydown[K_MOUSE1] && keydown[K_MOUSE2]){
		if (hud){
			if (selected_hud){
				if (!strcmp(hud->name,selected_hud->name)){
					HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],va("remove link",hud->name),0,1,1);
				}else{
					status = HUD_Editor_Get_Angle(cursor_loc[0],cursor_loc[1],hud);
					// only 4 lockons atm left,right,top,bottom
					if (status == 1){
						HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],va("link to %s's right",hud->name),0,1,1);
					}else if (status == 2){
						HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],va("link to %s's top",hud->name),0,1,1);
					}else if (status == 3){
						HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],va("link to %s's left",hud->name),0,1,1);
					}else if (status == 4){
						HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],va("link to %s's bottom",hud->name),0,1,1);
					}
				}
			}else{
				HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],va("link to %s",hud->name),0,1,1);
			}
		}else{
			// drawing fixed stuff
			Draw_OutlineRGB(bx,by,bw,bh,0,1,0,1);
			HUD_Editor_Draw_Tooltip(cursor_loc[0],cursor_loc[1],va("link to %s",bname),0,1,1);
		}
	}

	// reached max do nothing
	/*if (dontmove)
		return;
		*/
	// left mousebutton down = lets move it !
	if (keydown[K_MOUSE1] && !keydown[K_MOUSE2] && hud){
		HUD_Editor_Move(cursor_loc[0],cursor_loc[1],mouse_x,mouse_y,hud);
		last_moved_hud = hud;
		HUD_Recalculate();
		return;
	}
	// both mousebuttons down lets scale it !
	if (keydown[K_MOUSE1] && keydown[K_MOUSE2] && selected_hud == 0){
		if (hud->flags &9)
			return;
		cursor_loc[0] -= mouse_x;
		cursor_loc[1] -= mouse_y;
		scale = HUD_FindVar(hud,"scale");
		if (!scale)
			return;
		Cvar_Set(scale,va("%f",scale->value + (mouse_x+mouse_y)/200));
		if (scale->value <0)
			Cvar_Set(scale,"0");
		last_moved_hud = hud;
		HUD_Recalculate();
		return;
	}
	// if mouse 2 is pressed select the current hud for relinking
	if (!keydown[K_MOUSE1] && keydown[K_MOUSE2]){
		last_moved_hud = NULL;
		if (selected_hud == 0){
			selected_hud = hud;
			return;
		}else if (hud){
			center[0] = hud->lx + hud->lw/2;
			center[1] = hud->ly + hud->lh/2;
			// draw redline from center ouf selected to cursor
			Draw_AlphaLineRGB(center[0],center[1],cursor_loc[0],cursor_loc[1],1,0,0,1,1);
			last_moved_hud = 0;
			return;
		}else {
			// we havent found a hud so return;
			return;
		}

	}

	// both keys unpressed and a selected hud
	if (!keydown[K_MOUSE1] && !keydown[K_MOUSE2] && selected_hud != 0) {
		last_moved_hud = NULL;
		// remove the current link if we are still hovering above the same hud and set the defaults for screen alignment
		if (selected_hud == hud){
			Cvar_Set(hud->place,"screen");
			Cvar_Set(hud->pos_x,va("%f",cursor_loc[0]));
			Cvar_Set(hud->pos_y,va("%f",cursor_loc[1]));
			Cvar_Set(hud->align_x,"left");
			Cvar_Set(hud->align_y,"top");
			selected_hud = NULL;
		}else if (hud || strlen(bname)){
			if (hud){
				Cvar_Set(selected_hud->place,hud->name);
				status = HUD_Editor_Get_Angle(cursor_loc[0],cursor_loc[1],hud);
				// only 4 lockons atm left,right,top,bottom
				if (status == 1){
					Cvar_Set(selected_hud->align_x,"after");
					Cvar_Set(selected_hud->align_y,"center");
				}else if (status == 2){
					Cvar_Set(selected_hud->align_x,"center");
					Cvar_Set(selected_hud->align_y,"before");
				}else if (status == 3){
					Cvar_Set(selected_hud->align_x,"before");
					Cvar_Set(selected_hud->align_y,"center");
				}else if (status == 4){
					Cvar_Set(selected_hud->align_x,"center");
					Cvar_Set(selected_hud->align_y,"after");
				}
			}else{
				// we lock it to the extra stuff
				Cvar_Set(selected_hud->place,bplace);
				center[0] = bx + bw/2;
				center[1] = by + bh/2;
				Cvar_Set(selected_hud->place,bplace);
				Cvar_Set(selected_hud->align_x,balignx);
				Cvar_Set(selected_hud->align_y,baligny);
			}
			Cvar_Set(selected_hud->pos_x,"0");
			Cvar_Set(selected_hud->pos_y,"0");
		}

		selected_hud = NULL;
		HUD_Recalculate();
		return;
		}
	last_moved_hud = NULL;
	return;
}

void HUD_Editor_Draw(void) {
	if (!hud_editor.value)
		return;
	HUD_Editor();
}

void HUD_Editor_Init(void) {
	Cvar_Register(&hud_editor);
}
