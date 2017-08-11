Thre is a rewritten which is in active developement: https://github.com/RSDuck/craftus_reloaded

# Craftus
Craftus in a Minecraft clone for Nintendo 3DS(and not a continuation of 3DSCraft by smea). 

![Some screenshot](https://raw.githubusercontent.com/RSDuck/Craftus3DS/master/zscreenshots/scr_44_TOP_RIGHT.png)
![Another screenshot](https://raw.githubusercontent.com/RSDuck/Craftus3DS/master/zscreenshots/scr_47_TOP_LEFT.png)

## Features!
* A world which is theoretically infinite which looks a little bit like a landscape
* Stereo 3D
* Building and placing blocks
* Ambient Occlusion(these shadows in the edges between the cubes)

## Upcoming features
* A hand/the current item
* Trees, water, stuff that makes landscapes interesting
* Saving and opening worlds
* (Maybe) support for Minecraft Ressourcepacks or atleast a custom format and a converter for PC

## Note on building
Craftus needs a patched version of Citro3D to build(I going to do a pull request soon, but before this happends I have to improve the code quality to the standards of Citro3D)

## License
Apache License Version 2.0, see LICENSE and NOTICE

## Credits
* Of course to those who made 3DS homebrew possible(too many to be listed here)
* Everybody who wrote something for 3DS and made it open source
* The textures come from the Minecraft Ressourcepack Pixel Perfect by XSSheep which is licensed under CC-BY-SA. The file "Pixel Perfection V3.5.zip" is a full copy of it. The folder romfs/textures contains some textures extracted from it. For more informations about the Ressourcepack see here: http://www.minecraftforum.net/forums/mapping-and-modding/resource-packs/1242533-pixel-perfection-freshly-updated-1-10
* Craftus also depends on several other small but very useful libraries packaged in the deps folder
* The greedy meshing algorithm is based of the work from Robert O'Leary and Mikola Lysenko(see here: https://github.com/roboleary/GreedyMesh)

I called the folder zscreenshot because I want to have in my editor(VSCode) to be lower than the source folder
