const connectionStatus = document.getElementById('connectionStatus')
const statusEl = document.getElementById('status')
const nameInput = document.getElementById('nameInput')
const blinkingSlider = document.getElementById('blinkingSlider')
const blinkingNumber = document.getElementById('blinkingNumber')
const wifiConfig = document.getElementById('wifiConfig')
const ssidInput = document.getElementById('ssidInput')
const passwordInput = document.getElementById('passwordInput')
const togglePasswordBtn = document.getElementById('togglePasswordBtn')
const saveBtn = document.getElementById('saveBtn')
const reloadBtn = document.getElementById('reloadBtn')
const measurementsContainer = document.getElementById('measurementsContainer')


function setStatus(msg) {
    statusEl.textContent = msg
}

blinkingSlider.addEventListener('input', () => {
    blinkingNumber.value = blinkingSlider.value
})
blinkingNumber.addEventListener('input', () => {
    blinkingSlider.value = blinkingNumber.value
})

async function loadConfig() {
    setStatus('Loading configuration...')
    saveBtn.disabled = true
    try {
        const response = await fetch('/api/config')
        if (!response.ok) throw new Error(`HTTP error: ${response.status}`)
        const data = await response.json()

        nameInput.value = data?.name?.trim() || 'Web-Host-MCU'
        const blinking = Number.isFinite(data?.blinking_ms) ? data.blinking_ms : 250
        blinkingSlider.value = blinking
        blinkingNumber.value = blinking

        ssidInput.value = data?.wifi_ssid?.trim() || ''
        passwordInput.value = data?.wifi_pass?.trim() || ''

        saveBtn.disabled = false
        setStatus('Configuration loaded.')
    } catch (err) {
        nameInput.value = 'Web-Host-MCU'
        blinkingSlider.value = 250
        blinkingNumber.value = 250
        ssidInput.value = ''
        passwordInput.value = ''
        saveBtn.disabled = false
        setStatus('Failed to load configuration, using defaults.')
    }
}

async function saveConfig() {
    const config = {
        name: nameInput.value.trim() || 'Web-Host-MCU',
        blinking_ms: parseInt(blinkingSlider.value, 10) || 250,
        wifi_ssid: ssidInput.value.trim() || '',
        wifi_pass: passwordInput.value.trim() || ''
    }
    setStatus('Saving configuration...')
    saveBtn.disabled = true
    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        })
        if (!response.ok) throw new Error(`HTTP error: ${response.status}`)
        setStatus('Configuration saved successfully.')
    } catch (err) {
        setStatus('Failed to save configuration.')
    } finally {
        saveBtn.disabled = false
    }
}

async function loadStatus() {
    try {
        const res = await fetch('/api/status')
        if (!res.ok) throw new Error(`HTTP error: ${res.status}`)
        const data = await res.json()

        if (data?.network?.connected) {
            wifiConfig.style.display = 'none'
            connectionStatus.style.backgroundColor = 'green'
            connectionStatus.title = 'Wi-Fi connected'
            setStatus('Network connected. Wi-Fi config hidden.')
        } else {
            wifiConfig.style.display = 'block'
            connectionStatus.style.backgroundColor = 'orange'
            connectionStatus.title = 'Wi-Fi disconnected'
            setStatus('Network not connected. Configure Wi-Fi.')
        }
    } catch (err) {
        wifiConfig.style.display = 'block'
        connectionStatus.style.backgroundColor = 'red'
        connectionStatus.title = 'Device disconnected'
        setStatus('Failed to fetch network status. Wi-Fi config visible.')
    }
}


function renderMeasurements(list) {
    if (!Array.isArray(list) || list.length === 0) {
        measurementsContainer.textContent = 'No measurements available.'
        return
    }
    measurementsContainer.innerHTML = list.map(item => {
        const name = item.name ?? 'Unknown'
        const value = item.value ?? 'N/A'
        const unit = item.unit ?? ''
        return `<div class="measurement-item">
                            <span class="measurement-name">${name}</span>
                            <span class="measurement-value">${value} ${unit}</span>
                        </div>`
    }).join('')
}

async function fetchMeasurements() {
    try {
        const res = await fetch('/api/meas')
        if (!res.ok) throw new Error(`HTTP error: ${res.status}`)
        const data = await res.json()
        renderMeasurements(data)
    } catch (err) {
        measurementsContainer.textContent = 'Failed to load measurements.'
    }
}

saveBtn.addEventListener('click', saveConfig)
reloadBtn.addEventListener('click', loadConfig)
togglePasswordBtn.addEventListener('click', () => {
  const isHidden = passwordInput.type === 'password'
  passwordInput.type = isHidden ? 'text' : 'password'
  togglePasswordBtn.textContent = isHidden ? 'Hide' : 'Show'
})

loadStatus().then(() => {
    loadConfig()
})

fetchMeasurements()
setInterval(fetchMeasurements, 1000)
setInterval(loadStatus, 1200)
