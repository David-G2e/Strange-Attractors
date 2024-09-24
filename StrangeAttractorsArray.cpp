/***********************************************************************
StrangeAttractors1 - Program modeling the Lorentz Attractor with 1000 particles.
Data is exchanged between a background animation thread and the foreground 
rendering thread using a triple buffer, and retained-mode OpenGL rendering
using vertex and index buffers. Data is stored as a vector and accessed 
using pointers. 

Last Modified: 8/21/24
***********************************************************************/

#include <unistd.h>
#include <iostream>
#include <Threads/Thread.h>
#include <Threads/TripleBuffer.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Math/Random.h>
#include <GL/gl.h>
#include <GL/GLGeometryVertex.h>
#include <GL/GLVertexBuffer.h>
#include <vector>
#include <Vrui/Vrui.h>
#include <Vrui/Application.h>

class Animation:public Vrui::Application
	{
	/* Embedded classes: */
	private:
	typedef GLGeometry::Vertex<void,0,GLubyte,4,void,float,3> ParticleVertex; // Type for Particles storing colors and positions
	typedef std::vector<ParticleVertex> ParticleList; // Vector of particleVertex
	typedef GLVertexBuffer<ParticleVertex> VertexBuffer; // Type for OpenGL buffers holding mesh vertices
	
	/* Elements: */
	private:
	int initParticleSize; // number of Particles
	Threads::TripleBuffer<ParticleList> particleVertices;
	const ParticleList* opl; //array of particles from the last step
	VertexBuffer vertexBuffer; // Buffer holding mesh vertices
	Threads::Thread animationThread; // Thread object for the background animation thread
	
	/* Private methods: */
	void updateMesh(ParticleList& pl); // Recalculates all ParticleVertex
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

void Animation::updateMesh(Animation::ParticleList& pl)
	{
	/* Update the [x,y,z] coordinate of all Particles: */
	pl.clear();
	float s = 10;
	float r = 28;
	float b = 2.667;
	
	for(ParticleList::const_iterator oplIt=opl->begin();oplIt!=opl->end();++oplIt)
		{
		/* initial x,y,z coordinates */
		ParticleVertex pv = *oplIt;
		float x = pv.position[0];
		float y = pv.position[1];
		float z = pv.position[2];
		
		/* change at each time step for the Lorenz attractor system: */
		ParticleVertex::Position::Vector dot;
		dot[0]=s*(y-x);
		dot[1]=x*(r-z)-y;
		dot[2]=(x*y)-(b*z);
		float t = 0.003;
		
		/* updated positions: */
		pv.position+= dot*t;
		
		pl.push_back (pv);
		}
	
	//work from step for new iteration
	opl = &pl;
	}

void* Animation::animationThreadMethod(void)
	{
	while(true)
		{
		/* Sleep for approx. 1/60th of a second: */
		usleep(1000000/60);
		
		/* Start a new value in the mesh triple buffer: */
		ParticleList& pl=particleVertices.startNewValue();

		/* Recalculate the mesh vertices in the new triple buffer slot: */
		updateMesh(pl);
		
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
	initParticleSize = 1000;
	ParticleList& pl = particleVertices.startNewValue();

	for(int i = 0; i< initParticleSize ;++i)
		{
		/* Initialize the random position of Particles: */
		ParticleVertex pv;
		
		pv.position[0]=Math::randUniformCO(-20.0f,20.0f);
		pv.position[1]=Math::randUniformCO(-20.0f,20.0f);
		pv.position[2]=Math::randUniformCO(-20.0f,20.0f);
		
	
		/* Initialize the random color of Particles: */
		pv.color[0]=Math::randUniformCO(32,256);
		pv.color[1]=Math::randUniformCO(32,256);
		pv.color[2]=Math::randUniformCO(32,256);
		pv.color[3]=255;
		
		pl.push_back (pv);
		}
	opl = &pl;
	
	/* Calculate the first full mesh state in a new triple buffer slot: */
	particleVertices.postNewValue();
	
	/* Start the background animation thread: */
	animationThread.start(this,&Animation::animationThreadMethod);
	}

Animation::~Animation(void)
	{
	/* Shut down the background animation thread: */
	animationThread.cancel();
	animationThread.join();
	}

	
void Animation::frame(void)
	{
	/* Check if there is a new entry in the triple buffer and lock it: */
	if(particleVertices.lockNewValue())
		{
			
		const ParticleList& pl=particleVertices.getLockedValue();
		/* Point the vertex buffer to the new mesh vertices: */
		vertexBuffer.setSource(pl.size(),pl.data());
		}
	}

void Animation::display(GLContextData& contextData) const
	{
	/* Save OpenGL state: */
	glPushAttrib(GL_ENABLE_BIT|GL_POINT_BIT);
	
	glDisable(GL_LIGHTING);
	glPointSize(3.0f);
	
	/* Bind the vertex buffer (which automatically uploads any new vertex data): */
	VertexBuffer::DataItem* vbdi=vertexBuffer.bind(contextData);
		
	vertexBuffer.draw(GL_POINTS, vbdi);
	
	/* Unbind the buffers: */
	vertexBuffer.unbind();
	
	/* Restore OpenGL state: */
	glPopAttrib();
	}

void Animation::resetNavigation(void)
	{
	/* Center and scale the object: */
	Vrui::setNavigationTransformation(Vrui::Point::origin,150.0);
	}

VRUI_APPLICATION_RUN(Animation)
