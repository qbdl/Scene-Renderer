#include<glm/glm.hpp>
#include"PT\Renderer.h"
#include<utility>
#include<iostream>
#include"PT\hittable.h"
#include"PT\PTCamera.h"
#include"PT\PTRay.h"
#include"PT\PTrandom.h"
#include"PT\PTMaterial.h"

using namespace PT;

void Renderer::write_color(const vec3& color, int samples_per_pixel,int pos) {
	auto r = color.x; 
	auto g = color.y;
	auto b = color.z;

	float scale = 1.0f / samples_per_pixel;

	r = sqrtf(scale * r);
	g = sqrtf(scale * g);
	b = sqrtf(scale * b);
	//r = scale * r;
	//g = scale * g;
	//b = scale * b;

	int index = (pos) * 3;

	writeMtx.lock();
	resultImage[index] = int(255 * clamp(r, 0.0, 1.0));
	resultImage[index+1] = int(255 * clamp(g, 0.0, 1.0));
	resultImage[index+2] = int(255 * clamp(b, 0.0, 1.0));
	++pixelCnt;
	writeMtx.unlock();
}

Renderer::Renderer(int samples,int max_depth) {
	world = std::make_shared<hittable_list>();
	camera = nullptr;
	this->samples = samples;
	this->max_depth = max_depth;
	this->pixelCnt = 0;
}

Renderer::~Renderer() {
	delete[] resultImage;
}

void Renderer::addCam(std::shared_ptr<Camera> cam) {
	camera = cam;
}

void Renderer::addObject(std::shared_ptr<hittable> object) {
	world->add(object);
}

void Renderer::render(int threadNum) {
	int w = camera->width;
	int h = camera->height;

	// allocate space 
	resultImage = new int[w * h * 3];

	int interval = h / threadNum;
	if (interval * threadNum != h) {
		interval += 1;
	}


	for (int i = 0; i < threadNum; i++) {
		int start = i * interval;
		int end = std::min(start + interval-1, h - 1);
		std::thread t(&Renderer::threadRender,this,start, end);
		threads.push_back(std::move(t));
	}
}

vec3 Renderer::rayColor(const Ray& r,int depth) {
	hitRecord rec;

	if (depth <= 0) {
		return vec3(0, 0, 0);
	}

	bool isHit = world->hit(r,0.001,infinity,rec);
	if (!isHit) {
		return background;
	}
	Ray scattered;
	vec3 attenuation; // color of the surface brdf
	vec3 emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);

	bool hasScatter = rec.mat_ptr->scatter(r, rec, attenuation, scattered);
	if (!hasScatter) {
		//float distAtten = std::min(1.0f, 1.0f / (rec.t * rec.t));
		//return emitted *distAtten;
		return emitted;
	}

	vec3 radiance = rayColor(scattered, depth - 1);

	//float NoL = dot(scattered.dir, rec.normal);
	float NoL = 1.0f;
	//std::cout << "Attenuation" << attenuation.x << ' ' << attenuation.y << ' ' << attenuation.z << '\n';
	//std::cout << "radiance: " << radiance.x << ' ' << radiance.y<<' ' << radiance.z << '\n';
	vec3 finalColor = emitted + attenuation * NoL * radiance;
	return finalColor;
}

void Renderer::threadRender(int start, int end) {
	int h = camera->height;
	int w = camera->width;

	int samples_per_pixel = this->samples;
	auto id = std::this_thread::get_id();

	for (int j = end; j >=start; j--) {
		if ((end - j) % 5 == 0) {
			printMtx.lock();
			std::cerr << "Thread: " << id << " Progress: " << (end - j) * 1.0 / (end - start) * 100 << " % \n";
			printMtx.unlock();
		}

		for (int i = 0; i < w; i++) {
			vec3 color(0, 0, 0);
			for (int s = 0; s < samples_per_pixel; s++) {
				auto u = (i + random_double()) / (w - 1);
				auto v = (j + random_double()) / (h - 1);
				Ray r = camera->get_ray(u, v);
				color = color + rayColor(r, max_depth);
			}
			
			int pos = j * w + i;
			write_color(color, samples_per_pixel,pos);
		}
	}
}
void Renderer::writeToFile(const std::string& filename) {
	for (int i = 0; i < threads.size(); i++) {
		threads[i].join();
	}
	
	int w = camera->width;
	int h = camera->height;
	freopen(filename.c_str(), "w", stdout);
	std::cout << "P3\n" <<w<< ' ' << h<< "\n255\n";

	for (int j = h-1; j >=0; j--) {
		for (int i = 0; i < w; i++) {
			int index = j * w + i; 
			std::cout << resultImage[3 * index] << ' ' << resultImage[3 * index + 1]
				<<' ' << resultImage[3 * index + 2] << '\n';
		}
	}
	fclose(stdout);
}
