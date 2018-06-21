#include <Windows.h>
#include <sstream>
#include <iostream> 
#include <math.h>  
#include "HackProcess.h"
#include <vector>
#include <algorithm>   

/*----------------AIMBOT RELATED CODE------------------*/
//Create our 'hooking' and process managing object
CHackProcess fProcess;  
using namespace std;  
//We use F6 to exit the hack
#define F6_Key 0x75
//right click
#define RIGHT_MOUSE 0x02
//Here we store the num of players and update it regularly to know how many enemies we are dealing with
//this saves us doing loops unless their necessary e.g. we have 12 players and still loop 32 times wasting our great resources
//This makes our triggerbot MUCH faster in general
int NumOfPlayers = 32;
const DWORD dw_PlayerCountOffs = 0x5CE10C;//Engine.dll

//The player base is VERY important so we know where our player info is at
//including current jump status so we can use force jumping making our bHop
const DWORD Player_Base = 0x53FB04;//0x00574560;
//The ATTACK address BELOW, WE WRITE 5 TO SHOOT OR 4 TO 
const DWORD dw_mTeamOffset = 0x98;//client
const DWORD dw_Health = 0x90;//client
//FOR the x coord we can use cl_pdump for m_vecOrigin Vector or just move around the map looking for different values
//e.g. to find y coordinate walk up ladder search up, walk down search down etc.
const DWORD dw_Pos = 0x25C;//client

//Enemy Vars including the enemy loop
const DWORD EntityPlayer_Base = 0x54D324;
//How far in memory is each enemy data
const DWORD EntityLoopDistance = 0x10;

//ViewAngles
//We find these by moving our mouse around constantly looking for changed/unchanged value,
//the alternative is to use cl_pdump 1 and search for the value assigned to m_angRotation vector
const DWORD dw_m_angRotation = 0x461A9C;
RECT m_Rect; 

//Set of initial variables you'll need
//Our desktop handle
HDC HDC_Desktop;
//Brush to paint ESP etc
HBRUSH EnemyBrush;
HFONT Font; //font we use to write text with



//We will use this struct throughout all other tutorials adding more variables every time
struct MyPlayer_t  
{ 
	DWORD CLocalPlayer; 
	int Team; 
	int Health; 
	//WorldToScreenMatrix_t WorldToScreenMatrix;
	float Position[3]; 
	void ReadInformation() 
	{
		// Reading CLocalPlayer Pointer to our "CLocalPlayer" DWORD. 
		ReadProcessMemory (fProcess.__HandleProcess, (PBYTE*)(fProcess.__dwordClient + Player_Base), &CLocalPlayer, sizeof(DWORD), 0);
		// Reading out our Team to our "Team" Varible. 
		ReadProcessMemory (fProcess.__HandleProcess, (PBYTE*)(CLocalPlayer + dw_mTeamOffset), &Team, sizeof(int), 0);
		// Reading out our Health to our "Health" Varible.     
		ReadProcessMemory (fProcess.__HandleProcess, (PBYTE*)(CLocalPlayer + dw_Health), &Health, sizeof(int), 0); 
		// Reading out our Position to our "Position" Varible. 
		ReadProcessMemory (fProcess.__HandleProcess, (PBYTE*)(CLocalPlayer + dw_Pos), &Position, sizeof(float[3]), 0); 

		//Here we find how many player entities exist in our game, through this we make sure to only loop the amount of times we need
		//when grabbing player data
		//Note that this call could be even better at a regular 15 or so seconds timer but performance shouldn't vary a great deal
		ReadProcessMemory (fProcess.__HandleProcess, (PBYTE*)(fProcess.__dwordEngine + dw_PlayerCountOffs), &NumOfPlayers, sizeof(int), 0); 
	}
}MyPlayer;    



//ENemy struct
struct PlayerList_t 
{
	DWORD CBaseEntity; 
	int Team; 
	int Health; 
	float Position[3]; 
	float AimbotAngle[3]; 
	char Name[39]; 

	void ReadInformation(int Player) 
	{
		// Reading CBaseEntity Pointer to our "CBaseEntity" DWORD + Current Player in the loop. 0x10 is the CBaseEntity List Size 
		//"client.dll"+00545204 //0x571A5204
		ReadProcessMemory (fProcess.__HandleProcess, (PBYTE*)(fProcess.__dwordClient + EntityPlayer_Base  + (Player * EntityLoopDistance)),&CBaseEntity, sizeof(DWORD), 0);
		// Reading out our Team to our "Team" Varible. 
		ReadProcessMemory (fProcess.__HandleProcess, (PBYTE*)(CBaseEntity + dw_mTeamOffset), &Team, sizeof(int), 0);
		// Reading out our Health to our "Health" Varible.     
		ReadProcessMemory (fProcess.__HandleProcess, (PBYTE*)(CBaseEntity + dw_Health), &Health, sizeof(int), 0); 
		// Reading out our Position to our "Position" Varible. 
		ReadProcessMemory (fProcess.__HandleProcess, (PBYTE*)(CBaseEntity + dw_Pos), &Position, sizeof(float[3]), 0); 
 	}
}PlayerList[32];  


//We receive the 2-D Coordinates the colour and the device we want to use to draw those colours with
//HDC so we know where to draw and brush because we need it to draw
void DrawFilledRect(int x, int y, int w, int h)
{
	//We create our rectangle to draw on screen
	RECT rect = { x, y, x + w, y + h }; 
	//We clear that portion of the screen and display our rectangle
	FillRect(HDC_Desktop, &rect, EnemyBrush);
}


void DrawBorderBox(int x, int y, int w, int h, int thickness)
{
	//Top horiz line
	DrawFilledRect(x, y, w, thickness);
	//Left vertical line
	DrawFilledRect( x, y, thickness, h);
	//right vertical line
	DrawFilledRect((x + w), y, thickness, h);
	//bottom horiz line
	DrawFilledRect(x, y + h, w+thickness, thickness);
}


//Here is where we draw our line from point A to Point B
void DrawLine(float StartX, float StartY, float EndX, float EndY, COLORREF Pen)
{
	int a,b=0;
	HPEN hOPen;
	// penstyle, width, color
	HPEN hNPen = CreatePen(PS_SOLID, 2, Pen);
	hOPen = (HPEN)SelectObject(HDC_Desktop, hNPen);
	// starting point of line
	MoveToEx(HDC_Desktop, StartX, StartY, NULL);
	// ending point of line
	a = LineTo(HDC_Desktop, EndX, EndY);
	DeleteObject(SelectObject(HDC_Desktop, hOPen));
}

//Draw our text with this function
void DrawString(int x, int y, COLORREF color, const char* text)
{	
	SetTextAlign(HDC_Desktop,TA_CENTER|TA_NOUPDATECP);

	SetBkColor(HDC_Desktop,RGB(0,0,0));
	SetBkMode(HDC_Desktop,TRANSPARENT);

	SetTextColor(HDC_Desktop,color);

	SelectObject(HDC_Desktop,Font);

	TextOutA(HDC_Desktop,x,y,text,strlen(text));

	DeleteObject(Font);
}


int main()
{
	//Do we have OUR CSS GAME?
	fProcess.RunProcess(); 

	//Our infinite loop will go here

	return 0;
}