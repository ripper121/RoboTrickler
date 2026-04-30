
1. **Check for an existing ESP-IDF solution before writing new code**  
   Review the example code of these paths first:
   - `C:\Users\ripper121\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.1.3\libraries`

      Reuse existing solutions when possible. If no suitable solution exists, implement new code in the style and structure of the examples.

2. **Keep the implementation small and practical**  
   Prefer C. Use C++ only when clearly justified and without unnecessary overhead. Implement only what is needed now.


   ## Standard workflow

Use this order for all relevant implementation work:

1. Check ESP-IDF examples and components
2. Define the smallest working scope
3. Keep board mapping and hardware assumptions centralized