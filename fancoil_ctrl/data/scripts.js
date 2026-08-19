// Fancoil Controller JavaScript

// Global variables
let fancoilList = [];
let currentTab = 'dashboard';
let refreshInterval;
let refreshRate = 10000; // 10 seconds

// Populate the change-address dropdowns client-side: 64 <option> elements
// generated on the ESP made the root page too big for its heap (truncated
// pages, ERR_CONTENT_LENGTH_MISMATCH on subsequent asset loads)
function populateAddressSelects() {
  const sourceSelect = document.getElementById('source-address');
  const targetSelect = document.getElementById('target-address');
  if (sourceSelect && sourceSelect.options.length <= 1) {
    const broadcastOption = document.createElement('option');
    broadcastOption.value = '0';
    broadcastOption.textContent = '0 — BROADCAST (all listening units! factory default)';
    sourceSelect.appendChild(broadcastOption);
    for (let i = 1; i <= 32; i++) {
      const option = document.createElement('option');
      option.value = String(i);
      option.textContent = String(i);
      sourceSelect.appendChild(option);
    }
  }
  if (targetSelect && targetSelect.options.length <= 1) {
    for (let i = 1; i <= 32; i++) {
      const option = document.createElement('option');
      option.value = String(i);
      option.textContent = String(i);
      targetSelect.appendChild(option);
    }
  }
}

// DOM ready function
document.addEventListener('DOMContentLoaded', function() {
  populateAddressSelects();

  // Initialize the application
  initApp();
  
  // Set up event listeners for tab navigation
  document.querySelectorAll('.tab-link').forEach(tab => {
    tab.addEventListener('click', function() {
      switchTab(this.dataset.tab);
    });
  });
  
  // Set up event listener for refresh rate change
  const refreshRateSelect = document.getElementById('refresh-rate');
  if (refreshRateSelect) {
    refreshRateSelect.addEventListener('change', function() {
      refreshRate = parseInt(this.value);
      restartRefreshInterval();
    });
  }
});

// Initialize the application
function initApp() {
  // Load the list of fancoils
  loadFancoilList()
    .then(() => {
      // Initialize the dashboard
      initDashboard();
      
      // Start the refresh interval
      startRefreshInterval();
    })
    .catch(error => {
      showError('Failed to load fancoil list: ' + error.message);
    });
}

// Load the list of fancoils
async function loadFancoilList() {
  try {
    const response = await fetch('/list');
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    fancoilList = await response.json();
    return fancoilList;
  } catch (error) {
    console.error('Error loading fancoil list:', error);
    throw error;
  }
}

// Initialize the dashboard
function initDashboard() {
  const dashboardContainer = document.getElementById('dashboard-content');
  if (!dashboardContainer) return;
  
  // Clear the dashboard
  dashboardContainer.innerHTML = '';
  
  if (fancoilList.length === 0) {
    dashboardContainer.innerHTML = `
      <div class="card">
        <div class="card-content">
          <p>No fancoils registered. Use the settings tab to register a new fancoil.</p>
        </div>
      </div>
    `;
    return;
  }
  
  // Create the fancoil grid
  const fancoilGrid = document.createElement('div');
  fancoilGrid.className = 'fancoil-grid';
  
  // Add a card for each fancoil
  fancoilList.forEach(address => {
    const fancoilCard = createFancoilCard(address);
    fancoilGrid.appendChild(fancoilCard);
  });
  
  dashboardContainer.appendChild(fancoilGrid);
  
  // Load the data for each fancoil
  updateAllFancoils();
}

// Create a fancoil card
function createFancoilCard(address) {
  const card = document.createElement('div');
  card.className = 'card fancoil-card';
  card.id = `fancoil-${address}`;
  
  card.innerHTML = `
    <div class="card-header fancoil-header">
      <span>Fancoil #${address}</span>
      <span id="timeout-${address}" style="display: none; color: #F44336; font-weight: bold; font-size: 12px;">NO RESPONSE</span>
      <span id="collision-${address}" style="display: none; color: #F44336; font-weight: bold; font-size: 12px;" title="Multiple units are answering on this address. Isolate one unit (power the others off) and re-address it.">ADDRESS COLLISION?</span>
      <div class="status-indicator status-offline" id="status-${address}"></div>
    </div>
    <div class="card-content">
      <div class="loading" id="loading-${address}"></div>
      <div class="fancoil-data" id="data-${address}" style="display: none;">
        <div class="temp-display">
          <span id="ambient-${address}">--.-</span><span class="temp-unit">°C</span>
        </div>
        <div class="fancoil-status">
          <div class="fancoil-status-item">
            <strong>Setpoint:</strong> <span id="setpoint-${address}">--.-</span>°C
          </div>
          <div class="fancoil-status-item">
            <strong>Mode:</strong> <span id="mode-${address}">--</span>
          </div>
          <div class="fancoil-status-item">
            <strong>Fan:</strong> <span id="speed-${address}">--</span>
          </div>
          <div class="fancoil-status-item">
            <strong>Swing:</strong> <span id="swing-${address}">--</span>
          </div>
          <div class="fancoil-status-item">
            <strong>Water:</strong> <span id="water-${address}">--.-</span>°C
          </div>
          <div class="fancoil-status-item">
            <strong>Valve:</strong> <span id="valve-${address}">--</span>
          </div>
        </div>
        <div class="fancoil-controls mt-2">
          <div class="form-group">
            <label class="form-label">Power</label>
            <label class="switch">
              <input type="checkbox" id="power-${address}" onchange="setPower(${address}, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div class="form-group">
            <label class="form-label">Temperature</label>
            <div class="temp-control">
              <button class="temp-btn" onclick="adjustTemperature(${address}, -0.5)">-</button>
              <span class="temp-value" id="setpoint-control-${address}">--.-</span>
              <button class="temp-btn" onclick="adjustTemperature(${address}, 0.5)">+</button>
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">Mode</label>
            <select class="form-select" id="mode-select-${address}" onchange="setMode(${address}, this.value)">
              <option value="COOLING">Cooling</option>
              <option value="HEATING">Heating</option>
              <option value="FAN_ONLY">Fan Only</option>
              <option value="AUTO">Auto</option>
            </select>
          </div>
          <div class="form-group">
            <label class="form-label">Fan Speed</label>
            <select class="form-select" id="speed-select-${address}" onchange="setFanSpeed(${address}, this.value)">
              <option value="AUTOMATIC">Auto</option>
              <option value="MIN">Min</option>
              <option value="NIGHT">Night</option>
              <option value="MAX">Max</option>
            </select>
          </div>
          <div class="form-group">
            <label class="form-label">Swing</label>
            <label class="switch">
              <input type="checkbox" id="swing-control-${address}" onchange="setSwing(${address}, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
        </div>
      </div>
    </div>
    <div class="card-footer">
      <button class="btn" onclick="refreshFancoil(${address})">Refresh</button>
      <button class="btn btn-warning" onclick="resetWaterFault(${address})">Reset Water Fault</button>
    </div>
  `;
  
  return card;
}

// Update all fancoils
function updateAllFancoils() {
  fancoilList.forEach(address => {
    updateFancoil(address);
  });
}

// Update a single fancoil
async function updateFancoil(address) {
  const loadingElement = document.getElementById(`loading-${address}`);
  const dataElement = document.getElementById(`data-${address}`);
  const statusIndicator = document.getElementById(`status-${address}`);
  
  if (!loadingElement || !dataElement || !statusIndicator) return;
  
  // Show loading indicator
  loadingElement.style.display = 'inline-block';
  dataElement.style.display = 'none';
  
  try {
    const response = await fetch(`/get?addr=${address}`);
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    const data = await response.json();

    // Update the status indicator; readTimeout means the unit has not
    // answered on the modbus recently, so show it as offline even though
    // the controller itself responded
    if (data.readTimeout) {
      statusIndicator.className = 'status-indicator status-offline';
    } else {
      statusIndicator.className = 'status-indicator status-online';
    }
    const timeoutBadge = document.getElementById(`timeout-${address}`);
    if (timeoutBadge) {
      timeoutBadge.style.display = data.readTimeout ? 'inline' : 'none';
    }
    const collisionBadge = document.getElementById(`collision-${address}`);
    if (collisionBadge) {
      collisionBadge.style.display = data.collisionSuspected ? 'inline' : 'none';
    }
    
    // Update the fancoil data
    document.getElementById(`ambient-${address}`).textContent = data.ambient.toFixed(1);
    document.getElementById(`setpoint-${address}`).textContent = data.setpoint.toFixed(1);
    document.getElementById(`setpoint-control-${address}`).textContent = data.setpoint.toFixed(1);
    document.getElementById(`mode-${address}`).textContent = data.mode;
    document.getElementById(`speed-${address}`).textContent = data.speed;
    document.getElementById(`swing-${address}`).textContent = data.swing ? 'On' : 'Off';
    document.getElementById(`water-${address}`).textContent =
      (typeof data.waterTemp === 'number') ? data.waterTemp.toFixed(1) : '--.-';
    document.getElementById(`valve-${address}`).textContent = data.ev1 ? 'Open' : 'Closed';
    
    // Update the controls
    document.getElementById(`power-${address}`).checked = data.on;
    document.getElementById(`mode-select-${address}`).value = data.mode;
    document.getElementById(`speed-select-${address}`).value = data.speed;
    document.getElementById(`swing-control-${address}`).checked = data.swing;
    
    // Add mode indicator class
    const modeElement = document.getElementById(`mode-${address}`);
    modeElement.className = '';
    modeElement.classList.add('mode-indicator');
    
    if (data.mode === 'COOLING') {
      modeElement.classList.add('mode-cool');
      modeElement.textContent = 'COOL';
    } else if (data.mode === 'HEATING') {
      modeElement.classList.add('mode-heat');
      modeElement.textContent = 'HEAT';
    } else if (data.mode === 'FAN_ONLY') {
      modeElement.classList.add('mode-fan');
      modeElement.textContent = 'FAN';
    } else {
      modeElement.classList.add('mode-auto');
      modeElement.textContent = 'AUTO';
    }
    
    // Show the data
    loadingElement.style.display = 'none';
    dataElement.style.display = 'block';
    
  } catch (error) {
    console.error(`Error updating fancoil ${address}:`, error);
    statusIndicator.className = 'status-indicator status-offline';
    loadingElement.style.display = 'none';
    dataElement.style.display = 'block';
  }
}

// Refresh a single fancoil
function refreshFancoil(address) {
  updateFancoil(address);
}

// Set power state
async function setPower(address, state) {
  try {
    const response = await fetch(`/set?addr=${address}&on=${state}`, {
      method: 'POST'
    });
    
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    // Update the fancoil data
    updateFancoil(address);
    
  } catch (error) {
    console.error(`Error setting power for fancoil ${address}:`, error);
    showError(`Failed to set power for fancoil ${address}: ${error.message}`);
    
    // Revert the checkbox state
    document.getElementById(`power-${address}`).checked = !state;
  }
}

// Set mode
async function setMode(address, mode) {
  try {
    const response = await fetch(`/set?addr=${address}&mode=${mode}`, {
      method: 'POST'
    });
    
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    // Update the fancoil data
    updateFancoil(address);
    
  } catch (error) {
    console.error(`Error setting mode for fancoil ${address}:`, error);
    showError(`Failed to set mode for fancoil ${address}: ${error.message}`);
  }
}

// Set fan speed
async function setFanSpeed(address, speed) {
  try {
    const response = await fetch(`/set?addr=${address}&speed=${speed}`, {
      method: 'POST'
    });
    
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    // Update the fancoil data
    updateFancoil(address);
    
  } catch (error) {
    console.error(`Error setting fan speed for fancoil ${address}:`, error);
    showError(`Failed to set fan speed for fancoil ${address}: ${error.message}`);
  }
}

// Set swing
async function setSwing(address, state) {
  try {
    const response = await fetch(`/set?addr=${address}&swing=${state}`, {
      method: 'POST'
    });
    
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    // Update the fancoil data
    updateFancoil(address);
    
  } catch (error) {
    console.error(`Error setting swing for fancoil ${address}:`, error);
    showError(`Failed to set swing for fancoil ${address}: ${error.message}`);
    
    // Revert the checkbox state
    document.getElementById(`swing-control-${address}`).checked = !state;
  }
}

// Adjust temperature
async function adjustTemperature(address, delta) {
  const setpointElement = document.getElementById(`setpoint-control-${address}`);
  if (!setpointElement) return;
  
  const currentSetpoint = parseFloat(setpointElement.textContent);
  if (isNaN(currentSetpoint)) return;
  
  const newSetpoint = Math.min(Math.max(currentSetpoint + delta, 16), 30);
  
  try {
    const response = await fetch(`/set?addr=${address}&setpoint=${newSetpoint}`, {
      method: 'POST'
    });
    
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    // Update the fancoil data
    updateFancoil(address);
    
  } catch (error) {
    console.error(`Error setting temperature for fancoil ${address}:`, error);
    showError(`Failed to set temperature for fancoil ${address}: ${error.message}`);
  }
}

// Reset water temperature fault
async function resetWaterFault(address) {
  try {
    const response = await fetch(`/resetWaterTemperatureFault?addr=${address}`);
    
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    // Update the fancoil data
    updateFancoil(address);
    
    showMessage(`Water fault reset for fancoil ${address}`);
    
  } catch (error) {
    console.error(`Error resetting water fault for fancoil ${address}:`, error);
    showError(`Failed to reset water fault for fancoil ${address}: ${error.message}`);
  }
}

// Register a new fancoil
async function registerFancoil() {
  const addressInput = document.getElementById('register-address');
  if (!addressInput) return;
  
  const address = parseInt(addressInput.value);
  if (isNaN(address) || address < 1 || address > 32) {
    showError('Invalid address. Please enter a number between 1 and 32.');
    return;
  }
  
  try {
    const response = await fetch('/register', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded'
      },
      body: `addr=${address}`
    });
    
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    // Reload the fancoil list
    await loadFancoilList();
    
    // Reinitialize the dashboard
    initDashboard();
    
    showMessage(`Fancoil ${address} registered successfully`);
    
    // Clear the input
    addressInput.value = '';
    
  } catch (error) {
    console.error(`Error registering fancoil:`, error);
    showError(`Failed to register fancoil: ${error.message}`);
  }
}

// Unregister a fancoil
async function unregisterFancoil() {
  const addressInput = document.getElementById('unregister-address');
  if (!addressInput) return;
  
  const address = parseInt(addressInput.value);
  if (isNaN(address) || address < 1 || address > 32) {
    showError('Invalid address. Please enter a number between 1 and 32.');
    return;
  }
  
  try {
    const response = await fetch('/unregister', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded'
      },
      body: `addr=${address}`
    });
    
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    // Reload the fancoil list
    await loadFancoilList();
    
    // Reinitialize the dashboard
    initDashboard();
    
    showMessage(`Fancoil ${address} unregistered successfully`);
    
    // Clear the input
    addressInput.value = '';
    
  } catch (error) {
    console.error(`Error unregistering fancoil:`, error);
    showError(`Failed to unregister fancoil: ${error.message}`);
  }
}

// Change fancoil address
async function changeFancoilAddress() {
  const sourceInput = document.getElementById('source-address');
  const targetInput = document.getElementById('target-address');
  if (!sourceInput || !targetInput) return;
  
  const sourceAddress = parseInt(sourceInput.value);
  const targetAddress = parseInt(targetInput.value);

  if (isNaN(sourceAddress) || sourceAddress < 0 || sourceAddress > 32) {
    showError('Please select a source address.');
    return;
  }

  if (isNaN(targetAddress) || targetAddress < 1 || targetAddress > 32) {
    showError('Please select a target address.');
    return;
  }

  if (sourceAddress === targetAddress) {
    showError('Source and target address are identical.');
    return;
  }

  if (sourceAddress === 0) {
    if (!confirm('Address 0 is the modbus BROADCAST address: EVERY unit listening on the bus will take the new address, and none of them will confirm. Only proceed if exactly one unit is powered on the bus. Continue?')) {
      return;
    }
  }

  try {
    const response = await fetch('/changeAddress', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded'
      },
      body: `sourceAddress=${sourceAddress}&targetAddress=${targetAddress}`
    });

    const responseText = await response.text();

    if (!response.ok) {
      throw new Error(responseText || `HTTP error ${response.status}`);
    }

    showMessage(`Address change ${sourceAddress} → ${targetAddress}: ${responseText}`);

    // Clear the inputs
    sourceInput.value = '';
    targetInput.value = '';

  } catch (error) {
    console.error(`Error changing fancoil address:`, error);
    showError(`Failed to change fancoil address: ${error.message}`);
  }
}

// Factory reset
async function factoryReset() {
  if (!confirm('Are you sure you want to perform a factory reset? This will remove all registered fancoils.')) {
    return;
  }
  
  try {
    const response = await fetch('/factoryReset', {
      method: 'POST'
    });
    
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    
    // Reload the fancoil list
    await loadFancoilList();
    
    // Reinitialize the dashboard
    initDashboard();
    
    showMessage('Factory reset completed successfully');
    
  } catch (error) {
    console.error(`Error performing factory reset:`, error);
    showError(`Failed to perform factory reset: ${error.message}`);
  }
}

// Switch tab
function switchTab(tabId) {
  // Update the current tab
  currentTab = tabId;
  
  // Update the active tab link
  document.querySelectorAll('.tab-link').forEach(tab => {
    tab.classList.remove('active');
  });
  document.querySelector(`.tab-link[data-tab="${tabId}"]`).classList.add('active');
  
  // Update the active tab content
  document.querySelectorAll('.tab-content').forEach(content => {
    content.classList.remove('active');
  });
  document.getElementById(`${tabId}-content`).classList.add('active');
  
  // Initialize the tab content if needed
  if (tabId === 'dashboard') {
    initDashboard();
  } else if (tabId === 'statistics') {
    loadStatistics();
  }
}

// Load statistics
async function loadStatistics() {
  const statisticsContainer = document.getElementById('statistics-content');
  if (!statisticsContainer) return;
  
  try {
    const [readCount, readErrors, errorRatio, uptime] = await Promise.all([
      fetch('/modbusReadCount').then(res => res.text()),
      fetch('/modbusReadErrors').then(res => res.text()),
      fetch('/modbusErrorRatio').then(res => res.text()),
      fetch('/uptime').then(res => res.text())
    ]);
    
    statisticsContainer.innerHTML = `
      <div class="card">
        <div class="card-header">
          System Statistics
        </div>
        <div class="card-content">
          <div class="fancoil-status">
            <div class="fancoil-status-item">
              <strong>Uptime:</strong> ${formatUptime(uptime)}
            </div>
            <div class="fancoil-status-item">
              <strong>Modbus Read Count:</strong> ${readCount}
            </div>
            <div class="fancoil-status-item">
              <strong>Modbus Read Errors:</strong> ${readErrors}
            </div>
            <div class="fancoil-status-item">
              <strong>Error Ratio:</strong> ${errorRatio}
            </div>
          </div>
        </div>
      </div>
    `;
    
  } catch (error) {
    console.error('Error loading statistics:', error);
    statisticsContainer.innerHTML = `
      <div class="card">
        <div class="card-content">
          <p>Failed to load statistics: ${error.message}</p>
        </div>
      </div>
    `;
  }
}

// Format uptime
function formatUptime(seconds) {
  seconds = parseInt(seconds);
  const days = Math.floor(seconds / 86400);
  seconds %= 86400;
  const hours = Math.floor(seconds / 3600);
  seconds %= 3600;
  const minutes = Math.floor(seconds / 60);
  seconds %= 60;
  
  return `${days}d ${hours}h ${minutes}m ${seconds}s`;
}

// Show error message
function showError(message) {
  const errorElement = document.getElementById('error-message');
  if (!errorElement) return;
  
  errorElement.querySelector('.card-content').textContent = message;
  errorElement.style.display = 'block';
  
  // Hide the error message after 5 seconds
  setTimeout(() => {
    errorElement.style.display = 'none';
  }, 5000);
}

// Show success message
function showMessage(message) {
  const messageElement = document.getElementById('success-message');
  if (!messageElement) return;
  
  messageElement.querySelector('.card-content').textContent = message;
  messageElement.style.display = 'block';
  
  // Hide the message after 5 seconds
  setTimeout(() => {
    messageElement.style.display = 'none';
  }, 5000);
}

// Start the refresh interval
function startRefreshInterval() {
  // Clear any existing interval
  if (refreshInterval) {
    clearInterval(refreshInterval);
  }
  
  // Start a new interval
  refreshInterval = setInterval(() => {
    if (currentTab === 'dashboard') {
      updateAllFancoils();
    } else if (currentTab === 'statistics') {
      loadStatistics();
    }
  }, refreshRate);
}

// Restart the refresh interval
function restartRefreshInterval() {
  startRefreshInterval();
}

// Stop the refresh interval
function stopRefreshInterval() {
  if (refreshInterval) {
    clearInterval(refreshInterval);
    refreshInterval = null;
  }
}

// Debug functions
let quickDebugRegs = [0,1,8,9,15,16,101,102,103,104,105,108,198,199,200,201,202,203,204,205,206,207,209,210,211,212,213,215,216,218,219,221,222,224,231,233,235,236,237,238,245,246];

async function load(url, retry) { 
  try { 
    retry = retry || 5; 
    let x = await fetch(url); 
    let t = await x.text(); 
    if (t != null) return t; 
    else throw 'retry'; 
  } catch(e) {
    return load(url, retry -1);
  }
}

async function loadNr(url, retry) {
  let x = await load(url, retry); 
  let r = parseInt(x); 
  if (isNaN(r)) return await loadNr(url, (retry || 5) - 1); 
  else return r
}

async function loadReg(addr, reg, retry) {
  if (retry <= 0) return -1; 
  retry = retry || 5; 
  let x = await fetch("/read?addr=" + addr + "&reg=" + reg + "&len=1"); 
  if (x.status == 200) {
    let t = await x.text(); 
    if (t != null && t.indexOf('dec: ') > -1) { 
      let valS = t.substr(t.indexOf("dec:") + 5); 
      let val = parseInt(valS); 
      return val; 
    } else return loadReg(addr, reg, retry - 1);
  } else {
    return loadReg(addr, reg, retry - 1);
  }
}

async function debug(registers) {
  if (typeof registers === 'undefined') {
    registers=[];
    for (let i = 0; i <= 254; i++){
      registers.push(i)
    }
  }
  let html="<table><thead><tr><td>Addr</td><td>Val (dec)<td></tr></thead><tbody>"; 
  let s = document.getElementById('debugOut'); 
  let e = document.getElementById('debugAddress'); 
  let a = e.value; 
  let ret = {}; 
  for (let i of registers) { 
    s.innerText = 'Loading register ' + i; 
    ret[i] = await loadReg(a, i); 
    html += "<tr><td>" + i + "</td><td>" + ret[i] + "</td></tr>";
  } 
  ret['errorRatio'] = await load('/modbusErrorRatio'); 
  html +="</tbody></table>Base64: " + btoa(JSON.stringify(ret)); 
  s.innerHTML = html;
}