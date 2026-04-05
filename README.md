
# Gaussian Cake
## How to Run
Download the release, unzip and run the menv.exe.

### Input Bindings
ESC — Close application

R — Reset camera to default position

Left Drag — Rotate view (azimuth + incline)

Right Drag — Zoom in/out

To import your own .obj model: Add to the "meshes" folder.

## Inspiration
This project is inspired by the mathematical problem of **Optimal Transport (OT)**, which seeks a mapping between two distributions that minimizes a specified transport cost. 

Think about this question: "If I have a pile of stuff in one shape, and I want to rearrange it into another shape, what is the cheapest way to move it?"

![Alt text](https://www.microsoft.com/en-us/research/wp-content/uploads/2020/09/OTDD_Shovel-Figure.png)

And if you have ever heard of this website: https://obamify.com/, it shares a similar concept but instead, this program is 3D.

## What it does
This is an experimental graphic project that mainly serves the purpose of applying the math concept of Optimal Transport into an actual computer animation pipeline.

A low-level C++ OpenGL program that takes in **any .obj imported models** : A and B. Once you click "Bake It!", the program shows a **smooth animation of the model morphing from A to B**, or a morph of a distribution sampling from A to B. This is a particle system animation project that showcases the concept of optimal transport in two ways: GreedyDiscreteOT for the mesh and Gaussian OT for the distribution.

The Greedy discrete optimal transport is a simple approximation that allows the model to go from A to B.

The Gaussian optimal transport samples the mean and variance from the model, which is the particle-shaped blob that moves into another set of particle-shaped blobs.

_Since the main concept has this connection with the Gaussian distribution, the morphing process is similar to "baking", the callbacks to the idea of a Gaussian cake._

## How I built it

I only use raw code C++ and libraries(basic OpenGL, imgui UI, linear algebra libraries) for this project. I first initialized a shader, a window, and a basic mesh import. I first worked on the particle system, which allows later stages of implementation for the Gaussian sampling and applying optimal transport. Then I expand it to greedy optimal transport for the actual animation of model morphing.

## More about the Math: 

The Gaussian optimal transport samples the mean and variance from the model, which yields better runtime and exact geometry morphing with cheap computation power: It's only linear algebra!

\\(T(x)=m2​+A(x−m1​)\\)

where m1 and m2 are the Gaussian means, and A encodes the covariance-based linear transformation.

However,  the Greedy OT offers a simpler approach, looks better in animation, but it loses accuracy:

\\(\sum_{i} \| x_i - y_{\sigma(i)} \|^2_{\text{Greedy}}\\)

This mapping can then be directly used to generate smooth mesh-to-mesh morphing animations.
And therefore, this transformation could be simulated with computer animation, which could be expanded to mesh-to-mesh morphing animations.

![Alt text](https://d112y698adiu2z.cloudfront.net/photos/production/software_photos/004/531/959/datas/original.jpg)

More insight about OT in general: https://alexhwilliams.info/itsneuronalblog/2020/10/09/optimal-transport/

## Challenges I ran into

Debugging in C++ is painful. I had no clue why my program was not building at all-- 3hrs of debug just to realize my CMakeList.txt was poorly written.

## Accomplishments that I'm proud of
It actually worked.

## What I learned
- Different methods of Optimal Transport.
- A more solid understanding of Barycentric coordinates: how the particles are from a mesh through a triangle mesh.
- Constructing the 3D Gaussian formula using glm and eigen. (C++ libraries)
- The animation pipeline, using the OT method, is applied to the particle system.

## What's next for Gaussian Cake

This is an experimental project that provides insight into 3D model morphing, and I can definitely incorporate the concept into larger game projects or animation programs. 

_The point of this project is to show how theoretical concepts can be applied in diverse settings and how practical applications can help visualize and understand these concepts._


Bear model credit: https://cse125.ucsd.edu/data/bear.html 
