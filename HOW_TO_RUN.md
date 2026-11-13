# How to Run on Windows: Simple Steps

**Quickest start:** extract the folder to your Desktop, then double-click **`CacheSim Studio.bat`**.

This folder holds the whole project: the C++ code, the tests, the results, the **CacheSim Studio** program, the report and the slides.

| You want to... | Do this |
|---|---|
| **Open the program** | Double-click **`CacheSim Studio.bat`**. The first time, it also puts a **CacheSim Studio** shortcut on your Desktop |
| Build, test and run the C++ engine | Double-click **`run_windows.bat`** (needs a C++ compiler, see below) |
| Read the report | Open `docs\Project_Report.pdf` |
| Show the slides | Open `docs\Presentation.pptx` |
| See the full technical details | Open `README.md` |

> **First, unzip the folder.** If you got a `.zip` file, right-click it, choose **Extract All...**, and pick your **Desktop**. Do not run anything from inside the zip.

---

## CacheSim Studio (works immediately, nothing to install)

Double-click **`CacheSim Studio.bat`** (or the Desktop shortcut). It opens in its own window using Microsoft Edge, which is built into Windows.

| Page | What you can do |
|---|---|
| **Home** | All the C++ results as charts and a colour-coded table |
| **Simulator** | Choose a workload, cache size, number of requests and policies, press **Run**, and compare. Live charts, run history, **Export CSV** |
| **Live Race** | Watch all policies handle the same requests side by side. **Play**, step with the arrow keys, type your own requests |
| **AI Lab** | Move 4 sliders to build your own eviction rule and see its score instantly, or let the genetic algorithm evolve one |
| **Workloads** | See what each kind of traffic looks like and which policy wins |
| **How it works** | Steps, design patterns, speed table and class diagram |

The badge at the bottom left (**"Browser engine matches C++ (36/36)"**) means the app re-checked its numbers against the C++ program and they all agree.

### The assistant (optional): "Ask CacheSim"

The panel on the right answers questions about caches, the policies and **what is on your screen** (for example, "Explain my last simulation").

1. Create an API key at **console.anthropic.com** (API keys).
2. In CacheSim Studio, click the **gear** in the assistant panel, paste the key, click **Save key**.

- The key is saved **only inside the app on your PC**. It is never written into the project folder, so you can share the folder safely.
- To keep costs very low it uses **Claude Haiku 5.5** with **low effort**, **thinking off**, answers of at most **500 tokens**, and only the **last 6 messages** as memory. A typical question costs about **$0.0001**. The panel shows the running total.
- Everything else in CacheSim Studio works without a key.

---

## Build and run the C++ program

**Step 1. Install a free C++ compiler (one time only).** Pick **one**:
- **Visual Studio Community** (recommended): https://visualstudio.microsoft.com/vs/community/
  In the installer, tick **"Desktop development with C++"**, then click **Install**.
- **or MinGW g++ from MSYS2**: https://www.msys2.org/
  Then add `C:\msys64\ucrt64\bin` to your PATH.

**Step 2. Double-click `run_windows.bat`.**
- It finds your compiler by itself. You do **not** need CMake or a special command prompt.
- It builds the program, runs all 43 tests, prints the comparison table, refreshes the app's data and opens CacheSim Studio.
- At the end, press **Y** to also open the text menu, or **N** to finish.
- If Windows shows "Windows protected your PC", click **More info**, then **Run anyway**.

**Step 3 (optional). Run it again later.**
Open the project folder, click the File Explorer address bar, type `cmd` and press Enter. Then run:
```
build\cache_sim.exe
```

**Prefer an IDE?** In Visual Studio choose **File > Open > Folder...** and pick this folder. Visual Studio reads `CMakeLists.txt` by itself. Choose `cache_sim.exe` as the startup item and press the green Run button.

---

## Useful commands (after building)

| Command | What it does |
|---|---|
| `build\cache_sim.exe` | opens a simple text menu |
| `build\cache_sim.exe --compare-all` | compares all 7 policies on all workloads |
| `build\cache_sim.exe --trace --capacity 3` | shows every step of a small example |
| `build\cache_sim.exe --train` | re-trains the AI (takes about 20 seconds) |
| `build\cache_sim.exe --evaluate` | tests the AI on new, unseen workloads |
| `build\cache_sim.exe --benchmark` | measures speed |
| `build\cache_sim.exe --export-web` | refreshes the data CacheSim Studio shows (`web\data.js`) |
| `build\cache_sim.exe --help` | lists all options |

All results are saved as CSV files in the `results` folder. You can open them in Excel.

---

## If something goes wrong

| Problem | Fix |
|---|---|
| "No C++ compiler was found" | Do Step 1, then double-click `run_windows.bat` again |
| "BUILD FAILED" | Read `build\build_log.txt`. Make sure the folder was fully unzipped first |
| CacheSim Studio says "No simulation data yet" | Double-click `run_windows.bat` once, then press F5 in the app |
| CacheSim Studio does not open | Open `web\index.html` in Edge or Chrome instead (same program, in a browser tab) |
| Assistant: "That API key was not accepted" | Click the gear and paste a valid key from console.anthropic.com |
| Assistant: "Could not reach Anthropic" | Check your internet connection. The rest of the app works offline |
| "No trained weights" message | Not a problem. Keep `weights.txt` in the folder, or run `--train` once |
| Results CSV not written | Run the program from inside the project folder |
