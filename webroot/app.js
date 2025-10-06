const statusEl = document.getElementById('status')
const nameInput = document.getElementById('nameInput')
const blinkingSlider = document.getElementById('blinkingSlider')
const blinkingNumber = document.getElementById('blinkingNumber')
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
        const name = data && typeof data.name === 'string' && data.name.trim() !== '' ? data.name : 'Web-Host-MCU'
        const blinking = data && Number.isFinite(data.blinking_ms) ? data.blinking_ms : 250
        nameInput.value = name
        blinkingSlider.value = blinking
        blinkingNumber.value = blinking
        saveBtn.disabled = false
        setStatus('Configuration loaded.')
    } catch (err) {
        nameInput.value = 'Web-Host-MCU'
        blinkingSlider.value = 250
        blinkingNumber.value = 250
        saveBtn.disabled = false
        setStatus('Failed to load configuration, using defaults.')
    }
}

async function saveConfig() {
    const config = {
        name: nameInput.value.trim() || 'Web-Host-MCU',
        blinking_ms: parseInt(blinkingSlider.value, 10) || 250
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

function renderMeasurements(list) {
    if (!Array.isArray(list) || list.length === 0) {
        measurementsContainer.textContent = 'No measurements available.'
        return
    }

    const html = list.map(item => {
        const name = item.name ?? 'Unknown'
        const value = item.value ?? 'N/A'
        const unit = item.unit ?? ''
        return `<div class="measurement-item">
                  <span class="measurement-name">${name}</span>
                  <span class="measurement-value">${value} ${unit}</span>
                </div>`
    }).join('')
    measurementsContainer.innerHTML = html
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
loadConfig()

fetchMeasurements()
setInterval(fetchMeasurements, 1000)
