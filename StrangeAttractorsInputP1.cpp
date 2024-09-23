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
#include <Threads/RingBuffer.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Math/Random.h>
#include <GL/gl.h>
#include <GL/GLGeometryVertex.h>
#include <GL/GLVertexBuffer.h>
#include <vector>
#include <Vrui/Tool.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/ToolManager.h>
#include <Vrui/Vrui.h>
#include <Vrui/Application.h>

class Animation:public Vrui::Application
	{
	/* Embedded classes: */
	private:
	typedef GLGeometry::Vertex<void,0,GLubyte,4,void,float,3> ParticleVertex; // Type for Particles storing colors and positions
	typedef std::vector<ParticleVertex> ParticleList; // Vector of particleVertex
	typedef GLVertexBuffer<ParticleVertex> VertexBuffer; // Type for OpenGL buffers holding mesh vertices
	class SeedParticlesTool; // Forward declaration
	typedef Vrui::GenericToolFactory<SeedParticlesTool> SeedParticlesToolFactory; // Tool class uses the generic factory class	

	/* Elements: */
	private:
	int initParticleSize; // number of Particles
	Threads::TripleBuffer<ParticleList> particleVertices;
	const ParticleList* opl; //pointer to array of particles from the last step
	const ParticleList* apl; //pointer to array of particles from the foreground thread
	VertexBuffer vertexBuffer; // Buffer holding mesh vertices
	Threads::Thread animationThread; // Thread object for the background animation thread
	
	class SeedParticlesTool:public Vrui::Tool,public Vrui::Application::Tool<Animation>// The custom tool class, derived from application tool class
		{
		friend class Vrui::GenericToolFactory<SeedParticlesTool>;
		
		/* Elements: */
		private:
		static SeedParticlesToolFactory* factory; // Pointer to the factory object for this class
		
		/* Constructors and destructors: */
		public:
		static void initClass(void); // Initializes the custom tool's factory class
		SeedParticlesTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment);
		
		/* Methods: */
		virtual const Vrui::ToolFactory* getFactory(void) const;
		virtual void buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData);
		};
	
	/* Private methods: */
	void updateMesh(ParticleList& pl); // Recalculates all ParticleVertex
	ParticleList& addParticles(ParticleList& pl, const ParticleList* apl); // Adds ParticlesList apl to ParticleList pl (post Execution)
	void* animationThreadMethod(void); // Thread method for the background animation thread
	/* Constructors and destructors: */
	public:
	Animation(int& argc,char**& argv);
	virtual ~Animation(void);
	
	/* Methods from Vrui::Application: */
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void resetNavigation(void);
	
	/* Methods from seedParticlesTool: */
	void selectApplicationObject(void); // Dummy method to show how custom tools can interact with the application
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
	//pl = addParticles(pl, apl);
	//read the values from the ring buffer, then pushback
	
	//work from step for new iteration
	opl = &pl;
	}

Animation::ParticleList& Animation::addParticles(ParticleList& pl, const ParticleList* apl)
	{
		for(ParticleList::const_iterator aplIt=apl->begin();aplIt!=apl->end();++aplIt)
		{
			ParticleVertex pv = *aplIt;
			pv.color[0]=Math::randUniformCO(32,256);
			pv.color[1]=Math::randUniformCO(32,256);
			pv.color[2]=Math::randUniformCO(32,256);
			pv.color[3]=255;
			pl.push_back (pv);
		}
		//apl->clear();
		return pl;
	}

void* Animation::animationThreadMethod(void)
	{
	while(true)
		{
		/* Sleep for approx. 1/60th of a second: */
		usleep(1000000/60);
		
		/* Start a new value in the mesh triple buffer: */
		// should stay the same even with the addition
		ParticleList& pl=particleVertices.startNewValue();

		/* Recalculate the mesh vertices in the new triple buffer slot: */
		//here is where new particles should be added 
		updateMesh(pl);
		
		/* Push the new triple buffer slot to the foreground thread: */
		particleVertices.postNewValue();
		
		/* Wake up the foreground thread by requesting a Vrui frame immediately: */
		//here you probably want to see if there is something that needs to be updated
		Vrui::requestUpdate();
		}
	
	return 0;
	}

Animation::Animation(int& argc,char**& argv)
	:Vrui::Application(argc,argv)
	{
	SeedParticlesTool::initClass();
	initParticleSize = 100;
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
		//write new values into the ring buffer 
		
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

/***************************************************
Static elements of class VruiCustomToolDemo::MyTool:
***************************************************/

Animation::SeedParticlesToolFactory* Animation::SeedParticlesTool::factory=0;

/*******************************************
Methods of class VruiCustomToolDemo::MyTool:
*******************************************/

void Animation::SeedParticlesTool::initClass(void)
	{
	/* Create a factory object for the custom tool class: */
	factory=new SeedParticlesToolFactory("MyTool","Demo Application Tool",0,*Vrui::getToolManager());
	
	/* Set the custom tool class' input layout: */
	factory->setNumButtons(2,true); // Needs two buttons and can take optional buttons
	factory->setButtonFunction(0,"Does nothing");
	factory->setButtonFunction(1,"Select Application Object");
	factory->setButtonFunction(2,"Optional Button");
	
	/* Register the custom tool class with Vrui's tool manager: */
	Vrui::getToolManager()->addClass(factory,Vrui::ToolManager::defaultToolFactoryDestructor);
	}

Animation::SeedParticlesTool::SeedParticlesTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment)
	:Vrui::Tool(factory,inputAssignment)
	{
	}

const Vrui::ToolFactory* Animation::SeedParticlesTool::getFactory(void) const
	{
	return factory;
	}

void Animation::SeedParticlesTool::buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	if(cbData->newButtonState) // Button has just been pressed
		{
		std::cout<<"MyTool: Button "<<buttonSlotIndex<<" has just been pressed"<<std::endl;
		
		/* Call an application method if the second button was pressed: */
		if(buttonSlotIndex==1)
			application->selectApplicationObject();
			Vrui::Point point = Vrui::Tool::getButtonDevicePosition(buttonSlotIndex);
			std::cout <<"Position: " <<point[0]<<" " <<point[1]<< " " << point[2]<<std::endl;
		}
	else // Button has just been released
		{
		std::cout<<"MyTool: Button "<<buttonSlotIndex<<" has just been released"<<std::endl;
		}
	}

/*******************************************
Methods of class VruiCustomToolDemo:
*******************************************/
void Animation::selectApplicationObject(void)
	{
	std::cout<<"VruiCustomToolDemo: selectApplicationObject has just been called"<<std::endl;
	//where the write function will be called?
	//move point position to this method? depends on the read and write function
	}
	
VRUI_APPLICATION_RUN(Animation)
