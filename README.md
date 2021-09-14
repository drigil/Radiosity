# Radiosity

Codebase Used - https://github.com/simon-frankau/radiosity

### Added Camera to navigate the scene
Use W, A, S, D and the mouse in the scene.
Collision Detection is also implemented.
Relevant code is in the camera.h file.

### Added Specularity to the scene
Specularity was also added to the scene.
Only quads with isSpecular to true will display specularity.
Each quad with isEmitter == True is treated as a light source. 
Depends on the location of the light source and the camera.

### Save the current scene
The screenshot of the initial scene is saved in the png folder by default.
To save additional screenshots, one can save the scene by pressing the 'C' key.
The screenshot gets saved in the png folder with the timestamp as the name of the image.

### Example scene

Image - 
![alt text](https://github.com/drigil/Radiosity/blob/master/png/scene.png)

GIF - 
![alt text](https://github.com/drigil/Radiosity/blob/master/png/scene_gif.gif)
