// OpenNI2Project.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
// General headers
#include <stdio.h>
// OpenNI2 headers
#include "OpenNI.h"
#include "NiTE.h"
using namespace openni;
// GLUT headers
#include <GLUT/GLUT.h>
#include <algorithm>
int window_w = 640;
int window_h = 480;
OniRGB888Pixel* gl_texture;
nite::UserTracker uTracker;

char ReadLastCharOfLine()
{
	int newChar = 0;
	int lastChar;
	fflush(stdout);
	do 
	{
		lastChar = newChar;
		newChar = getchar();
	}
	while ((newChar != '\n') 
		&& (newChar != EOF));
	return (char)lastChar;
}

bool HandleStatus(nite::Status status)
{
	if (status == nite::STATUS_OK)
		return true;
	printf("ERROR: #%d, %s", status,
		OpenNI::getExtendedError());
	ReadLastCharOfLine();
	return false;
}

void gl_KeyboardCallback(unsigned char key, int x, int y)
{
	if (key == 27) // ESC Key
	{
		uTracker.destroy();
		nite::NiTE::shutdown();
		exit(0);
	}
}

void gl_IdleCallback()
{
	glutPostRedisplay();
}

void gl_DisplayCallback()
{
	if (uTracker.isValid())
	{
		//Status status = STATUS_OK;
		nite::Status status = nite::STATUS_OK;
		nite::UserTrackerFrameRef usersFrame;
		status = uTracker.readFrame(&usersFrame);
		if (status == nite::STATUS_OK && usersFrame.isValid())
		{
			// Clear the OpenGL buffers
			glClear (
				GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
			// Setup the OpenGL viewpoint
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadIdentity();
			glOrtho(0, window_w, window_h, 0, -1.0, 1.0);

			// UPDATING TEXTURE (DEPTH 1MM TO RGB888)
			VideoFrameRef depthFrame = usersFrame.getDepthFrame();

			int depthHistogram[65536];
			int numberOfPoints = 0;
			memset(depthHistogram, 0,
				sizeof(depthHistogram));
			for	(int y = 0;
					y < depthFrame.getHeight(); ++y)
			{
				DepthPixel* depthCell = (DepthPixel*)(
					(char*)depthFrame.getData() + 
					(y * depthFrame.getStrideInBytes())
					);
				for	(int x = 0; x < depthFrame.getWidth();
						++x, ++depthCell)
				{
					if (*depthCell != 0)
					{
						depthHistogram[*depthCell]++;
						numberOfPoints++;
					}
				}
			}

			for (int nIndex=1;
			nIndex < sizeof(depthHistogram) / sizeof(int);
			nIndex++)
			{
				depthHistogram[nIndex] +=
					depthHistogram[nIndex-1];
			}

			int colors[] = {16777215,
				14565387, 32255, 7996159, 16530175, 8373026, 14590399, 7062435, 13951499, 55807};
			double resizeFactor = std::min(
				(window_w / (double)depthFrame.getWidth()),
				(window_h / (double)depthFrame.getHeight()));
			unsigned int texture_x = (unsigned int)(window_w - 
				(resizeFactor * depthFrame.getWidth())) / 2;
			unsigned int texture_y = (unsigned int)(window_h - 
				(resizeFactor * depthFrame.getHeight())) / 2;

			nite::UserMap usersMap = usersFrame.getUserMap();

			for	(unsigned int y = 0;
				y < (window_h - 2 * texture_y); ++y)
			{
				OniRGB888Pixel* texturePixel = gl_texture + 
					((y + texture_y) * window_w) + texture_x;
				for	(unsigned int x = 0;
					x < (window_w - 2 * texture_x);
					++x, ++texturePixel)
				{
					DepthPixel* depthPixel = 
						(DepthPixel*)(
							(char*)depthFrame.getData() + 
							((int)(y / resizeFactor) * 
								depthFrame.getStrideInBytes())
						) +	(int)(x / resizeFactor);
					nite::UserId* userPixel =
						(nite::UserId*)(
							(char*)usersMap.getPixels() + 
							((int)(y / resizeFactor) * 
								usersMap.getStride())
						) +	(int)(x / resizeFactor);
					if (*depthPixel != 0)
					{
						float depthValue = (1 - ((float)depthHistogram[*depthPixel]  / numberOfPoints));
						int userColor = colors[(int)*userPixel % 10];
						texturePixel->b = ((userColor / 65536) % 256) * depthValue;
						texturePixel->g = ((userColor / 256) % 256) * depthValue;
						texturePixel->r = ((userColor / 1) % 256) * depthValue;
					}
					else
					{
						texturePixel->b = 0;
						texturePixel->g = 0;
						texturePixel->r = 0;
					}
				}
			}

			// Create the OpenGL texture map
			glTexParameteri(GL_TEXTURE_2D,
				0x8191, GL_TRUE); // 0x8191 = GL_GENERATE_MIPMAP
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
				window_w, window_h,	0, GL_RGB,
				GL_UNSIGNED_BYTE, gl_texture);
						
			glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(0.0f, 0.0f, 0.0f);
			glTexCoord2f(0.0f, 1.0f);
			glVertex3f(0.0f, (float)window_h, 0.0f);
			glTexCoord2f(1.0f, 1.0f);
			glVertex3f((float)window_w,
				(float)window_h, 0.0f);
			glTexCoord2f(1.0f, 0.0f);
			glVertex3f((float)window_w, 0.0f, 0.0f);
			glEnd();

			const nite::Array<nite::UserData>& users =
				usersFrame.getUsers();
			glBegin( GL_POINTS );
			glColor3f( 1.f, 0.f, 0.f );
			for (int i = 0; i < users.getSize(); ++i)
			{
				if (users[i].isNew())
				{
					uTracker.startSkeletonTracking(
						users[i].getId());
				}
				nite::Skeleton user_skel = users[i].getSkeleton();
				if (user_skel.getState() == 
					nite::SKELETON_TRACKED)
				{
					for (int joint_Id = 0; joint_Id < 15;
						++joint_Id)
					{
						float posX, posY;
						status = 
						uTracker.convertJointCoordinatesToDepth(
							user_skel.getJoint((nite::JointType)
								joint_Id).getPosition().x,
							user_skel.getJoint((nite::JointType)
								joint_Id).getPosition().y,
							user_skel.getJoint((nite::JointType)
								joint_Id).getPosition().z,
							&posX, &posY);
						if (HandleStatus(status)){
							glVertex2f(
								(posX * resizeFactor) + texture_x,
								(posY * resizeFactor) + texture_y);
						}
					}
				}
			}
			glEnd();
			glColor3f( 1.f, 1.f, 1.f );
			glutSwapBuffers();
		}
	}
}

int main(int argc, char* argv[])
{
	printf("\r\n----------------- NiTE & User Tracker -------------------\r\n");
	nite::Status status = nite::STATUS_OK;
	printf("Initializing NiTE ...\r\n");
	status = nite::NiTE::initialize();
	if (!HandleStatus(status)) return 1;

	printf("Creating a user tracker object ...\r\n");
	status = uTracker.create();
	if (!HandleStatus(status)) return 1;
	
	printf("\r\n---------------------- OpenGL -------------------------\r\n");
	printf("Initializing OpenGL ...\r\n");
	gl_texture = (OniRGB888Pixel*)malloc(
		window_w * window_h * sizeof(OniRGB888Pixel));
	glutInit(&argc, (char**)argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(window_w, window_h);
	glutCreateWindow ("OpenGL | OpenNI 2.x Skeleton");
	glPointSize(10.0);
	glutKeyboardFunc(gl_KeyboardCallback);
	glutDisplayFunc(gl_DisplayCallback);
	glutIdleFunc(gl_IdleCallback);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_COLOR_MATERIAL);
	printf("Starting OpenGL rendering process ...\r\n");
	glutMainLoop();

	return 0;
}

