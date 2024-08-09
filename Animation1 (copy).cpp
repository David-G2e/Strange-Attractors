/***********************************************************************
Animation - Example program demonstrating data exchange between a
background animation thread and the foreground rendering thread using
a triple buffer, and retained-mode OpenGL rendering using vertex and
index buffers.
Copyright (c) 2014-2021 Oliver Kreylos

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include <unistd.h>
#include <iostream>
#include <Threads/Thread.h>
#include <Threads/TripleBuffer.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Math/Random.h>
#include <GL/gl.h>
#include <GL/GLMaterial.h>
#include <GL/GLGeometryVertex.h>
#include <GL/GLVertexBuffer.h>
#include <GL/GLIndexBuffer.h>
#include <Vrui/Vrui.h>
#include <Vrui/Application.h>

class Animation:public Vrui::Application
	{
	/* Embedded classes: */
	private:
	typedef GLGeometry::Vertex<void,0,GLubyte,4,void,float,3> ParticleVertex; // Type for Particles storing colors and positions
	typedef GLVertexBuffer<ParticleVertex> VertexBuffer; // Type for OpenGL buffers holding mesh vertices
	
	/* Elements: */
	private:
	int particleSize; // number of Particles
	Threads::TripleBuffer<ParticleVertex*> particleVertices;
	float phase; // Phase angle for mesh animation
	GLMaterial meshMaterialFront,meshMaterialBack; // Material properties to render the mesh from the front and back
	VertexBuffer vertexBuffer; // Buffer holding mesh vertices
	Threads::Thread animationThread; // Thread object for the background animation thread
	
	/* Private methods: */
	void updateMesh(ParticleVertex* pv); // Recalculates all ParticleVertex
	void* animationThreadMethod(void); // Thread method for the background animation thread
	
	/* Constructors and destructors: */
	public:
	Animation(int& argc,char**& argv);
	virtual ~Animation(void);
	
	/* Methods from Vrui::Application: */
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void resetNavigation(void);
	};

/**************************
Methods of class Animation:
**************************/

void Animation::updateMesh(Animation::ParticleVertex* pv)
	{
	/* Update the [x,y,z] coordinate of all Particles: */
	ParticleVertex* pPtr=pv;
	for(int i=0;i<particleSize;++i,++pPtr)
		{
		/* Initialize the random position of Particles: */
		pPtr->position[0]=Math::randUniformCO(-1.0f,1.0f);
		pPtr->position[1]=Math::randUniformCO(-1.0f,1.0f);
		pPtr->position[2]=Math::randUniformCO(-1.0f,1.0f);
		
		/* Initialize the random color of Particles: */
		pPtr->color[0]=Math::randUniformCO(128,256);
		pPtr->color[1]=Math::randUniformCO(128,256);
		pPtr->color[2]=Math::randUniformCO(128,256);
		pPtr->color[3]=255;
		}

	}

void* Animation::animationThreadMethod(void)
	{
	while(true)
		{
		/* Sleep for approx. 1/60th of a second: */
		usleep(1000000/60);
		
		/* Increment the phase angle by 1 radians/second (assuming we slept 1/60s) and wrap to [-pi, +pi): */
		phase=(phase+1.0f/60.0f);
		
		/* Start a new value in the mesh triple buffer: */
		ParticleVertex* pv=particleVertices.startNewValue();
		
		/* Recalculate the mesh vertices in the new triple buffer slot based on the current phase angle: */
		updateMesh(pv);
		
		/* Push the new triple buffer slot to the foreground thread: */
		particleVertices.postNewValue();
		
		/* Wake up the foreground thread by requesting a Vrui frame immediately: */
		Vrui::requestUpdate();
		}
	
	return 0;
	}

Animation::Animation(int& argc,char**& argv)
	:Vrui::Application(argc,argv)
	{
	/* Initialize the number of Particles: */
	particleSize=1000;
	
	for(int i=0;i<3;++i)
		{
		/* Access the i-th triple buffer slot: */
		ParticleVertex*& pv=particleVertices.getBuffer(i);
		
		/* Allocate the in-memory vertex array: */
		pv=new ParticleVertex[particleSize];
		}
		
	/* Initialize animation state: */
	phase=0.0f;
	
	/* Calculate the first full mesh state in a new triple buffer slot: */
	updateMesh(particleVertices.startNewValue());
	particleVertices.postNewValue();
	
	/* Start the background animation thread: */
	animationThread.start(this,&Animation::animationThreadMethod);
	}

Animation::~Animation(void)
	{
	/* Shut down the background animation thread: */
	animationThread.cancel();
	animationThread.join();
	
	/* Delete the in-memory vertex arrays in all three triple buffer slots: */
	for(int i=0;i<3;++i)
		delete[] particleVertices.getBuffer(i);
	}

void Animation::frame(void)
	{
	/* Check if there is a new entry in the triple buffer and lock it: */
	if(particleVertices.lockNewValue())
		{
		/* Point the vertex buffer to the new mesh vertices: */
		vertexBuffer.setSource(particleSize,particleVertices.getLockedValue());
		}
	}

void Animation::display(GLContextData& contextData) const
	{
	/* Save OpenGL state: */
	glPushAttrib(GL_ENABLE_BIT|GL_LIGHTING_BIT);
	
	/* Enable double-sided lighting: */
	glDisable(GL_CULL_FACE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
	
	/* Set up a surface material: */
	glMaterial(GLMaterialEnums::FRONT,meshMaterialFront);
	glMaterial(GLMaterialEnums::BACK,meshMaterialBack);
	
	/* Bind the vertex buffer (which automatically uploads any new vertex data): */
	vertexBuffer.bind(contextData);
	
	/* Unbind the buffers: */
	vertexBuffer.unbind();
	
	/* Restore OpenGL state: */
	glPopAttrib();
	}

void Animation::resetNavigation(void)
	{
	/* Center and scale the object: */
	Vrui::setNavigationTransformation(Vrui::Point::origin,9.0*Math::Constants<double>::pi,Vrui::Vector(0,1,0));
	}

VRUI_APPLICATION_RUN(Animation)
