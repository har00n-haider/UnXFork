# ***"Untitled" Project X***
Technical Fixes and Enhancements for ***Final Fantasy X | X-2 HD Remaster***

Originally envisioned as a quick-fix solution for a few technical problems in the otherwise excellent PC port of ***Final Fantasy X | X-2 HD Remaster***, this mod aims to customize some of the game's internal working systems such as cutscene skipping, UI and language selection. The "fix" part of the mod should still be very minimal, but there is still room to polish off many aspects of gameplay.

This mod is derived partially from work done on language selection by the Steam community; refer to `AUTHORS`. _If you find your name in there (Steam username) and would like to be credited differently, please let me know on Steam._

&nbsp;&nbsp;&nbsp;If you would like to donate for my work, I have a Pledgie campaign setup for this project.<br><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='https://pledgie.com/campaigns/31692'><img alt='Click here to lend your support to: &quot;Untitled&quot; Project X and make a donation at pledgie.com !' src='https://pledgie.com/campaigns/31692.png?skin_name=chrome' border='0' ></a>

<hr>

**NOTE:** _The code in this repository pertains only to ***unx.dll***._
>The source code for the ***dxgi.dll*** is in my [Special K repository](https://github.com/Kaldaien/SpecialK/). Compiling and distributing that project is complicated and I have not had time to simplify the process. If you wish to contribute to this project, you will have to contact me on Steam and I can walk you through getting that all done. It seems unlikely that anyone would need to work on that part of the project to contribute, it only handles non-game specific render stuff.


## Dev notes

### Quick start

1. Install "Platform tools v141" via visual studio installer
2. Intall the Windows 8.1 SDK
3. Clone [Dear IMGUI](https://github.com/ocornut/imgui.git) checked out at v1.50 (Makue sure to update include dir)
4. Clone [SpecialK](https://gitlab.com/Kaldaien/SpecialK.git) and checkout branch unx-soilmaster-update (commit 9e51df06, relating to version 0.8.60.2). Follow build instructions on that branch in README
5. Build SpecialK (to generate the dxgi.lib for unx to link to)
6. Build UnX

Also check that all absolute paths make sense (output dir, include dir etc.)
