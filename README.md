# Virtual Mall - OpenGL Project

This is a virtual mall application built with C++ and OpenGL. This guide provides detailed instructions for setting up the project from scratch using Visual Studio.

> **Note:** Throughout this guide, `D:\VirtualMall` is used as an example path. You can use any path you prefer, but you must be consistent and use your chosen path during the configuration steps.

## Prerequisites

*   **Visual Studio**: You must have Visual Studio installed with the **Desktop development with C++** workload.
*   **Git**: You need Git installed to clone the repository.

---

## Full Setup Guide (From Scratch)

These steps will guide you through creating a new Visual Studio project and configuring it to use the source code from this repository.

### STEP 1: Create a New Project in Visual Studio

1.  Open **Visual Studio**.
2.  Select **Create a new project**.
3.  In the search bar, type `Empty Project` and select the **Empty Project (C++)** template.
4.  Click **Next**.

### STEP 2: Set the Project Location

This is a critical step. To avoid issues later, set the location and name correctly.

1.  Set **Location** to your desired directory (e.g., `D:\`, `C:\Projects\`, etc.).
2.  Set **Project Name** to `VirtualMall`.

This will create the project structure at your chosen location (e.g., `D:\VirtualMall\`), containing the solution file (`VirtualMall.sln`) and a sub-folder for the project itself.

### STEP 3: Add the Source Code

Copy the `include`, `lib`, and `src` folders from this repository into your newly created project's root directory (e.g., `D:\VirtualMall\`).

Your folder structure should now look like this (using `D:\VirtualMall` as an example):
```
D:\VirtualMall\
├── include/
├── lib/
├── src/
├── VirtualMall.sln
└── VirtualMall/
    └── ... (Visual Studio project files)
```

### STEP 4: Add Source Files to the Project

Now, you need to tell Visual Studio which source files to compile.

1.  In Visual Studio, find the **Solution Explorer** panel.
2.  Right-click on your project (it should be named **VirtualMall**).
3.  Go to **Add** → **Existing Item...**.
4.  Navigate into the `src` folder and select both `main.cpp` and `glad.c`.
5.  Click **Add**.

### STEP 5: Configure Project Properties (Most Important Step)

This step links all the necessary libraries and headers.

1.  In the **Solution Explorer**, right-click on the **VirtualMall** project again and select **Properties**.
2.  **⚠️ IMPORTANT:** At the top of the Properties window, make sure you set:
    *   **Configuration**: `All Configurations`
    *   **Platform**: `x64`
    *   *Failing to do this will cause errors.*

3.  **A. Set the Include Path**
    *   Navigate to **C/C++** → **General**.
    *   Find **Additional Include Directories** and click the dropdown, then `<Edit...>`.
    *   Add a new line and enter the path to your `include` folder (e.g., `D:\VirtualMall\include`). **Remember to use your actual project path.**

4.  **B. Set the Library Path**
    *   Navigate to **Linker** → **General**.
    *   Find **Additional Library Directories** and click the dropdown, then `<Edit...>`.
    *   Add a new line and enter the path to your `lib` folder (e.g., `D:\VirtualMall\lib`). **Remember to use your actual project path.**

5.  **C. Link the Required Libraries**
    *   Navigate to **Linker** → **Input**.
    *   Find **Additional Dependencies** and click the dropdown, then `<Edit...>`.
    *   Add the following libraries, each on a new line:
        ```
        glfw3.lib
        opengl32.lib
        ```
6.  Click **Apply**, then **OK** to save the changes.

### STEP 6: Run the Project

Press **Ctrl + F5** (or go to `Debug` → `Start Without Debugging`) to build and run the project.

If everything was configured correctly, a window should open, and you will see the application running. This confirms that OpenGL is fully working! 🎉

## In-App Controls

*   **[WASD] + [SPACE]**: Move around freely.
*   **[T]**: Toggle Cinematic Tour Mode.
*   **[F]**: Toggle Flashlight.
