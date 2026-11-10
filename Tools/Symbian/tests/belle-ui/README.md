# Native browser dialogs

Exercises JavaScript prompts and file selection through the browser's native UI.
The upload fixture checks the selected sample file's name and contents.

```sh
python3 Tools/Symbian/tests/belle-ui/server.py \
  --host 0.0.0.0 --port 8782 --evidence /tmp/belle-ui-uploads.jsonl
```

Open `http://localhost:8782/` in the browser and follow the page's prompt and
file-picker actions. Copy the sample text file from this directory into the
emulated or device filesystem before choosing it.

The page displays the dialog results. Successful upload requests record matching
file contents in `/tmp/belle-ui-uploads.jsonl`.
