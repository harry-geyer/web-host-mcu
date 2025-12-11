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
const scanWifiBtn = document.getElementById('scanWifiBtn')
const ssidDropdown = document.getElementById('ssidDropdown')
const ssidLoader = document.getElementById('ssidLoader')


const measurementNames = {
    'relative_humidity': 'Relative Humidity',
    'temperature': 'Temperature',
}

const unitNames = {
    'celsius': 'ºC',
}


let lastStations = []


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
            //setStatus('Network not connected. Configure Wi-Fi.')
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

    measurementsContainer.innerHTML = ''

    list.forEach(item => {
        const rawName = item.name ?? 'Unknown'
        const rawUnit = item.unit ?? ''

        const displayName = measurementNames.hasOwnProperty(rawName)
            ? measurementNames[rawName]
            : rawName

        const displayUnit = unitNames.hasOwnProperty(rawUnit)
            ? unitNames[rawUnit]
            : rawUnit

        const value = (item.value === 0 || item.value) ? item.value : 'N/A'

        const div = document.createElement('div')
        div.className = 'measurement-item'

        const nameSpan = document.createElement('span')
        nameSpan.className = 'measurement-name'
        nameSpan.textContent = displayName

        const valueSpan = document.createElement('span')
        valueSpan.className = 'measurement-value'
        valueSpan.textContent = `${value}${displayUnit ? ' ' + displayUnit : ''}`

        div.appendChild(nameSpan)
        div.appendChild(valueSpan)
        measurementsContainer.appendChild(div)
    })
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

function renderSSIDDropdown(stations) {
    ssidDropdown.innerHTML = ''
    if (!stations || stations.length === 0) {
        ssidDropdown.style.display = 'none'
        return
    }

    stations.forEach(st => {
        const div = document.createElement('div')
        div.className = 'ssid-item'
        const signalWrapper = document.createElement('span')
        signalWrapper.innerHTML = getSignalIcon(st.rssi)
        const ssidText = document.createElement('span')
        ssidText.textContent = st.ssid || '(Unnamed)'
        ssidText.className = 'ssid-text'
        ssidText.title = `MAC: ${st.mac || 'Unknown'}\nChannel: ${st.channel || 'N/A'}`
        const lockIcon = document.createElement('span')
        lockIcon.className = 'ssid-lock'
        lockIcon.title = st.auth || ''
        lockIcon.textContent = (st.auth && st.auth !== 'OPEN') ? '🔒' : ''
        div.appendChild(signalWrapper)
        div.appendChild(ssidText)
        div.appendChild(lockIcon)
        div.addEventListener('click', () => {
            ssidInput.value = st.ssid
            ssidDropdown.style.display = 'none'
            if (st.auth === 'OPEN') {
                passwordInput.value = ''
                passwordInput.disabled = true
                passwordInput.placeholder = 'Open network (no password)'
                passwordInput.classList.add('disabled-input')
            } else {
                passwordInput.disabled = false
                passwordInput.placeholder = 'Enter Wi-Fi password'
                passwordInput.classList.remove('disabled-input')
            }
        })

        ssidDropdown.appendChild(div)
    })
    ssidDropdown.style.display = 'block'
    ssidDropdown.style.zIndex = 1000
}

function getSignalIcon(rssi) {
    let level = 0
    let text = 'Very Weak'
    if (rssi >= -40) {
        level = 4
        text = 'Excellent'
    } else if (rssi >= -55) {
        level = 3
        text = 'Good'
    } else if (rssi >= -70) {
        level = 2
        text = 'Fair'
    } else if (rssi >= -85) {
        level = 1
        text = 'Weak'
    } else {
        level = 0
        text = 'Very Weak'
    }
    return `
        <i class="icon__signal-strength signal-${level}" title="Signal strength: ${text}\nRSSI: ${rssi}">
            <span class="bar-1"></span>
            <span class="bar-2"></span>
            <span class="bar-3"></span>
            <span class="bar-4"></span>
        </i>
    `
}

document.addEventListener('click', (e) => {
    if (!ssidDropdown.contains(e.target) && !scanWifiBtn.contains(e.target) && !dropDownBtn.contains(e.target)) {
        ssidDropdown.style.display = 'none'
    }
})

saveBtn.addEventListener('click', saveConfig)
reloadBtn.addEventListener('click', loadConfig)
togglePasswordBtn.addEventListener('click', () => {
    const isHidden = passwordInput.type === 'password'
    passwordInput.type = isHidden ? 'text' : 'password'
    togglePasswordBtn.textContent = isHidden ? 'Hide' : 'Show'
})

scanWifiBtn.addEventListener('click', async () => {
    setStatus('Starting Wi-Fi scan...')
    ssidDropdown.style.display = 'none'
    ssidLoader.style.display = 'inline-block'
    try {
        const startRes = await fetch('/api/wifi-scan-start')
        if (!startRes.ok) throw new Error(`HTTP error: ${startRes.status}`)
        const startData = await startRes.json()
        if (startData.status !== 'ok') throw new Error('Scan start failed')

        setStatus('Scanning... Please wait 3 seconds...')
        await new Promise(r => setTimeout(r, 3000))

        const getRes = await fetch('/api/wifi-scan-get')
        if (!getRes.ok) {
            if (getRes.status === 409) setStatus('Scan not started.')
            else if (getRes.status === 425) setStatus('Scan not ready yet.')
            else throw new Error(`HTTP error: ${getRes.status}`)
            return
        }

        const data = await getRes.json()
        if (data.status !== 'ok' || !Array.isArray(data.stations)) {
            setStatus('No stations found.')
            return
        }

        lastStations = data.stations
        renderSSIDDropdown(lastStations)
        setStatus(`Found ${lastStations.length} networks.`)
    } catch (err) {
        console.error(err)
        setStatus('Failed to scan Wi-Fi.')
    } finally {
        ssidLoader.style.display = 'none'
    }
})

dropDownBtn.addEventListener('click', () => {
    if (!lastStations || lastStations.length === 0) {
        setStatus('No previous scan results. Please scan first.')
        return
    }
    renderSSIDDropdown(lastStations)
})

ssidInput.addEventListener('input', () => {
    passwordInput.disabled = false
    passwordInput.placeholder = 'Enter Wi-Fi password'
    passwordInput.classList.remove('disabled-input')
})

loadStatus().then(() => {
    loadConfig()
})

fetchMeasurements()
setInterval(fetchMeasurements, 1000)
setInterval(loadStatus, 1200)
