Import("env")

import os
try:
    import minify_html
except ImportError:
    env.Execute("$PYTHONEXE -m pip install minify-html")
    import minify_html

# Get absolute paths using the project directory
project_dir = env['PROJECT_DIR']
html_files_path = os.path.join(project_dir, "HTML/")
output_files_path = os.path.join(project_dir, "include/")


def to_c_array(source, target, name, shrink=False):
    # Check if the file is binary (e.g., .der) or text (e.g., .html)
    is_binary = os.path.splitext(source)[1].lower() in ['.der', '.crt', '.key']

    # Open the file in the appropriate mode
    if is_binary:
        with open(source, 'rb') as file:
            content = file.read()
    else:
        with open(source, 'r') as file:
            content = file.read()

        # Minify the HTML content if needed
        if shrink:
            content = minify_html.minify(content, minify_js=True, minify_css=True, remove_processing_instructions=True)

    # Start converting content to a C array
    c_array = f"#include <pgmspace.h>\n\nconst char {name}[] PROGMEM = {{\n"

    # Handle binary and text content differently
    if is_binary:
        for i, byte in enumerate(content):
            if i % 20 == 0:
                c_array += "\n"
            c_array += f"0x{byte:02x}, "
    else:
        for i, char in enumerate(content):
            if i % 20 == 0:
                c_array += "\n"
            c_array += f"0x{ord(char):02x}, "

    c_array = c_array.strip(', ')
    c_array += ", 0x00\n};\n"

    # Write the result to the target header file
    with open(target, 'w') as file:
        file.write(c_array)

def ProccessHTMLFiles():
    for HTML_FILE in os.listdir(html_files_path):
        filename = os.fsdecode(HTML_FILE)
        if filename.endswith(".html"):
            new_name = filename[:-5]
            new_name += "_html.h"
            INPUT_FILE = os.path.join(html_files_path, HTML_FILE)
            OUTPUT_FILE = os.path.join(output_files_path, new_name)
            to_c_array(INPUT_FILE, OUTPUT_FILE, new_name[:-2], True)
        if filename.endswith(".der"):
            new_name = filename[:-4]
            new_name += "_cert.h"
            INPUT_FILE = os.path.join(html_files_path, HTML_FILE)
            OUTPUT_FILE = os.path.join(output_files_path, new_name)
            to_c_array(INPUT_FILE, OUTPUT_FILE, new_name[:-2], False)

# Run the conversion before building
ProccessHTMLFiles()
