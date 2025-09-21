const GET_ENDPOINT = '/api/config';  // Replace with your actual GET URL
const POST_ENDPOINT = '/api/config'; // Replace with your actual POST URL

const statusEl = document.getElementById('status');
const editor = document.getElementById('configEditor');
const saveBtn = document.getElementById('saveBtn');
const reloadBtn = document.getElementById('reloadBtn');

// Fetch and load config
async function loadConfig() {
  setStatus('Loading configuration...');
  editor.disabled = true;
  saveBtn.disabled = true;
  try {
    const response = await fetch(GET_ENDPOINT);
    if (!response.ok) throw new Error(`HTTP error: ${response.status}`);
    const data = await response.json();
    editor.value = JSON.stringify(data, null, 2);
    editor.disabled = false;
    saveBtn.disabled = false;
    setStatus('Configuration loaded. You can edit and save.');
  } catch (error) {
    console.error(error);
    setStatus('Failed to load configuration.');
  }
}

// Save config to server
async function saveConfig() {
  try {
    const parsed = JSON.parse(editor.value); // Validate JSON
    setStatus('Saving configuration...');
    saveBtn.disabled = true;
    const response = await fetch(POST_ENDPOINT, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(parsed)
    });
    if (!response.ok) throw new Error(`HTTP error: ${response.status}`);
    setStatus('Configuration saved successfully!');
  } catch (error) {
    console.error(error);
    setStatus('Failed to save: Invalid JSON or server error.');
  } finally {
    saveBtn.disabled = false;
  }
}

// Helper to set status text
function setStatus(message) {
  statusEl.textContent = message;
}

// Event listeners
saveBtn.addEventListener('click', saveConfig);
reloadBtn.addEventListener('click', loadConfig);

// Initial load
loadConfig();
