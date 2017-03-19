#include <sstream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "application/application.h"
#include "common/log.h"
#include "stb_image.h"

#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include "rapidxml_print.hpp"

using namespace rapidxml;

Application::~Application() {
	if(simulation) delete simulation;
	if(scenegraph_root) {
		scenegraph::destroy(scenegraph_root);
		scenegraph_root = 0;
	}
	for(std::map<std::string, Image*>::iterator it = images.begin(); it != images.end(); ++it) {
		if(it->second) {
			if(it->second->data) {
				delete it->second->data;
				it->second->data = 0;
			}
			delete it->second;
			it->second = 0;
		}
	}
	images.clear();
}

#define WAVEFRONT_FILE "test.obj"

#ifndef DESKTOP_APP
Application::Application(AAssetManager* asset_manager) {
	this->asset_manager = asset_manager;
    assert(this->asset_manager);
	LOGI("Creating application\n");
	scenegraph_root = loadResources();
	assert(scenegraph_root);
	simulation = new Simulation();
	assert(simulation);
	init();
}

Image* loadImage(const char* filename, AAssetManager* asset_manager) {
	Image* image = new Image();
	assert(image);
	AAsset* file = AAssetManager_open(asset_manager, filename, AASSET_MODE_BUFFER);
	size_t file_length = AAsset_getLength(file);
	unsigned char* image_file_bytes = new unsigned char[file_length];
	AAsset_read(file, image_file_bytes, file_length);
	AAsset_close(file);
    image->data = stbi_load_from_memory(image_file_bytes, file_length * sizeof(unsigned char), &image->w, &image->h, &image->comp, STBI_default);
    if(image->data == 0) {
		LOGE("Unable to load image: %s", filename);
		return 0;
	}
	return image;
}

Node* Application::loadResources() {
	WavefrontSceneGraphFactory factory;
	bool status = factory.addWavefront(WAVEFRONT_FILE, glm::mat4(1.f), asset_manager);
	for(std::set<std::string>::iterator it = factory.textures.begin(); it != factory.textures.end(); ++it) {
		std::string s = *it;
		Image* t = loadImage(s.c_str(), asset_manager);
		assert(t);
		images[s] = t;
	}
	if(!status) {
		LOGE("Unable to load assets");
		return 0;
	} else {
		LOGI("Loaded wavefront from asset");
		return factory.build();
	}
}
#else
Application::Application() {
	scenegraph_root = loadResources();
	assert(scenegraph_root);
	simulation = new Simulation();
	assert(simulation);
	init();
}

std::stringstream loadFile(const char* filename) {
	std::ifstream file_stream(filename);
	std::stringstream ss;
	if (file_stream.is_open())
	{
		std::string line;
		while (std::getline(file_stream, line))
		{
			int pos;
			//line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
			ss << line;
		}
		file_stream.close();
	}
	else
	{
		LOGE("Unable to load %s", filename);
	}
	return ss;
}

Node* Application::loadResources() {
	WavefrontSceneGraphFactory factory;
	bool status = factory.addWavefront(WAVEFRONT_FILE, glm::mat4(1.f));
	for(std::set<std::string>::iterator it = factory.textures.begin(); it != factory.textures.end(); ++it) {
		std::string s = *it;
		Image* t = loadImage(s.c_str());
		images[s] = t;
	}
	if(!status) {
		LOGE("Unable to load assets");
		return 0;
	}
	
	Node* wf_root = factory.build();
	assert(wf_root);


	/* Create test xml
	xml_document<> doc;
	
	//xml_node<> *node = doc.allocate_node(node_element, "GroupNode", "Root Node");
	//doc.append_node(node);
	
	xml_node<> *wf_transform = doc.allocate_node(node_element, "WavefrontFile", "test.obj Library");
	//xml_attribute<> *attr = doc.allocate_attribute("matrix", "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0");
	wf_transform->append_attribute(doc.allocate_attribute("filename", "test.obj"));

	xml_node<> *wf_instances = doc.allocate_node(node_element, "InstanceNode", "Box.000");
	wf_instances->append_attribute(doc.allocate_attribute("matrix", "1 0 0 0 0 1 0 0 0 0 0 1 0 0 0 0 1"));
	wf_transform->append_node(wf_instances);

	xml_node<> *physics_node = doc.allocate_node(node_element, "PhysicsNode", "Box.000");
	physics_node->append_attribute(doc.allocate_attribute("mass", "1.0f"));
	xml_node<> *collision_shape = doc.allocate_node(node_element, "CollisionShape", "Box");
	collision_shape->append_attribute(doc.allocate_attribute("width", "1"));
	collision_shape->append_attribute(doc.allocate_attribute("height", "1"));
	collision_shape->append_attribute(doc.allocate_attribute("length", "1"));
	collision_shape->append_attribute(doc.allocate_attribute("offset_x", "0"));
	collision_shape->append_attribute(doc.allocate_attribute("offset_y", "0"));
	collision_shape->append_attribute(doc.allocate_attribute("offset_z", "0"));

	physics_node->append_node(collision_shape);

	doc.append_node(wf_transform);
	doc.append_node(physics_node);
	*/

	//std::string xml_data = loadFile("data.xml").str();
	rapidxml::file<> xml_file("data.xml");
	xml_document<> doc;
	doc.parse<0>(xml_file.data());

	std::stringstream ss;
	std::ostream_iterator<char> iter(ss);
	rapidxml::print(iter, doc, 0);
	LOGI("%s", ss.str().c_str());


	return wf_root;

}
#endif

void Application::init() {
	// Init camera
	camera = new Camera();
	assert(camera);
	camera->position.y = 4.0;
	camera->moveBackward(18.0);
	camera->update();

	// Init physics
	TransformNode* ground_node = scenegraph_root->find_transform_node("Plane");
	TransformNode* trans_node1 = scenegraph_root->find_transform_node("Cube.000");
	TransformNode* trans_node2 = scenegraph_root->find_transform_node("Cube.001");
	TransformNode* trans_node3 = scenegraph_root->find_transform_node("Cube.002");
	assert(ground_node);
	assert(trans_node1);
	assert(trans_node2);
	assert(trans_node3);
	simulation->addPhysicsBoxNode(ground_node, 0.f, 50.f, 50.f, 50.f, 0.0f, -50.0f, 0.0f);
	simulation->addPhysicsBoxNode(trans_node1, 1.f, 1.f, 1.f, 1.f);
	simulation->addPhysicsBoxNode(trans_node2, 1.f, 1.f, 1.f, 1.f);
	simulation->addPhysicsBoxNode(trans_node3, 1.f, 1.f, 1.f, 1.f);

	// Apply angular velocity to boxes
	for(std::map<int, PhysicsNode>::iterator it = simulation->physics_nodes.begin(); it != simulation->physics_nodes.end(); ++it) {
		if(it->second.transform_node == trans_node1) {
			btVector3 up_force(0.0, 350.0, 0.0);
			const btVector3 rel_pos(-0.1, 1.0, 0.1);
			it->second.body->applyForce(up_force, rel_pos);
			btVector3 angular_velocity(0.0, 0.0, -5.0);
			it->second.body->setAngularVelocity(angular_velocity);
		} else if(it->second.transform_node == trans_node2) {
			btVector3 up_force(0.0, 350.0, 0.0);
			const btVector3 rel_pos(0.0, 1.0, 0.0);
			it->second.body->applyForce(up_force, rel_pos);
			btVector3 angular_velocity(0.0, 10.0, 0.0);
			it->second.body->setAngularVelocity(angular_velocity);
		} else if(it->second.transform_node == trans_node3) {
			btVector3 up_force(0.0, 350.0, 0.0);
			const btVector3 rel_pos(0.0, 1.0, 0.0);
			it->second.body->applyForce(up_force, rel_pos);
			btVector3 angular_velocity(4.3, 0.0, 1.0);
			it->second.body->setAngularVelocity(angular_velocity);
		}
	}
}

void Application::resize(int width, int height) {
	assert(camera);
	camera->projectionMatrix = glm::perspective(45.0f, (float)((double)width / (double)height), 0.1f, 10000.0f);
	camera->update();
}

void Application::step() {
	grey += 0.01f;
	if (grey > 1.0f) {
		grey = 0.0f;
	}
	if(!simulation) return;

	simulation->step();

	// Update scenegraph with physics simulation data
	for (int j=simulation->dynamicsWorld->getNumCollisionObjects()-1; j>=0 ;j--)
	{
		btCollisionObject* obj = simulation->dynamicsWorld->getCollisionObjectArray()[j];
		btRigidBody* body = btRigidBody::upcast(obj);
		btTransform trans;
		if (body && body->getMotionState())
		{
			body->getMotionState()->getWorldTransform(trans);
		} else
		{
			trans = obj->getWorldTransform();
		}

		std::map<int, PhysicsNode>::iterator itr = simulation->physics_nodes.find(j);
		if(itr != simulation->physics_nodes.end() && itr->second.mass > 0.f) {
			btVector3 origin_bt = trans.getOrigin();
			glm::vec3 pos((float)origin_bt.x(), (float)origin_bt.y(), (float)origin_bt.z());
			glm::mat4 pos_mat = glm::translate(glm::mat4(1.0f), pos);

			btQuaternion rotation_bt = trans.getRotation();
			glm::quat quat(rotation_bt.w(), rotation_bt.x(), rotation_bt.y(), rotation_bt.z());
			glm::mat4 rotation = glm::toMat4(quat);
			itr->second.transform_node->matrix = pos_mat * rotation;
		}
	}
}
