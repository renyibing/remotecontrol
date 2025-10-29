(() => {
  'use strict';

  // DOM reference
  const remoteVideo = document.getElementById('remote_video');
  const dataTextInput = document.getElementById('data_text');
  remoteVideo.controls = true;

  // State
  let peerConnection = null;
  let dataChannel = null;
  let candidates = [];
  let hasReceivedSdp = false;

  // ICE server configuration
  const iceServers = [{ urls: 'stun:stun.l.google.com:19302' }];
  const peerConnectionConfig = { iceServers };

  // WebSocket configuration
  const isSSL = location.protocol === 'https:';
  const wsProtocol = isSSL ? 'wss://' : 'ws://';
  const wsUrl = wsProtocol + location.host + '/ws';
  const ws = new WebSocket(wsUrl);

  ws.onopen = () => {
    console.log('ws open()');
  };
  ws.onerror = (error) => {
    console.error('ws onerror() ERROR:', error);
  };
  ws.onmessage = (event) => {
    console.log('ws onmessage() data:', event.data);
    const message = JSON.parse(event.data);
    switch (message.type) {
      case 'offer': {
        console.log('Received offer ...');
        const offer = new RTCSessionDescription(message);
        console.log('offer: ', offer);
        setOffer(offer);
        break;
      }
      case 'answer': {
        console.log('Received answer ...');
        const answer = new RTCSessionDescription(message);
        console.log('answer: ', answer);
        setAnswer(answer);
        break;
      }
      case 'candidate': {
        console.log('Received ICE candidate ...');
        const candidate = new RTCIceCandidate(message.ice);
        console.log('candidate: ', candidate);
        if (hasReceivedSdp) {
          addIceCandidate(candidate);
        } else {
          candidates.push(candidate);
        }
        break;
      }
      case 'close': {
        console.log('peer connection is closed ...');
        break;
      }
      default:
        console.warn('Unknown WS message type:', message.type);
    }
  };

  // Build the codec list from the capabilities when the page is loaded
  try { populateVideoCodecSelect(); } catch (e) { console.warn('populateVideoCodecSelect failed:', e); }
  try { bindUi(); } catch (e) { console.warn('bindUi failed:', e); }

  // Public API
  function connect() {
    console.group();
    if (!peerConnection) {
      console.log('make Offer');
      makeOffer();
    } else {
      console.warn('peer connection already exists.');
    }
    console.groupEnd();
  }

  function disconnect() {
    console.group();
    if (peerConnection && peerConnection.iceConnectionState !== 'closed') {
      try {
        peerConnection.close();
      } catch (e) {
        console.warn('close() error:', e);
      }
      peerConnection = null;
      if (dataChannel) {
        try { dataChannel.close(); } catch (_) {}
        dataChannel = null;
      }
      // Notify the other side
      if (ws && ws.readyState === 1) {
        ws.send(JSON.stringify({ type: 'close' }));
        console.log('sending close message');
      }
      cleanupVideoElement(remoteVideo);
      candidates = [];
      hasReceivedSdp = false;
      try { updateUiState(); } catch (_) {}
    } else {
      console.log('peerConnection is closed.');
    }
    console.groupEnd();
  }

  function play() {
    remoteVideo.play();
  }

  function sendDataChannel() {
    const textData = dataTextInput.value;
    if (textData.length === 0) return;
    if (!dataChannel || dataChannel.readyState !== 'open') return;
    dataChannel.send(new TextEncoder().encode(textData));
    dataTextInput.value = '';
  }

  // Internal processing
  function drainCandidate() {
    hasReceivedSdp = true;
    candidates.forEach((candidate) => addIceCandidate(candidate));
    candidates = [];
  }

  function addIceCandidate(candidate) {
    if (!peerConnection) {
      console.error('PeerConnection does not exist!');
      return;
    }
    try {
      peerConnection.addIceCandidate(candidate);
    } catch (e) {
      console.error('addIceCandidate() failed:', e);
    }
  }

  function sendIceCandidate(candidate) {
    console.log('---sending ICE candidate ---');
    const message = JSON.stringify({ type: 'candidate', ice: candidate });
    console.log('sending candidate=' + message);
    ws.send(message);
  }

  function playVideo(element, stream) {
    element.srcObject = stream;
    // Update the UI according to the remote stream update
    try { updateUiState(); } catch (_) {}
  }

  // Update the UI according to the connection status
  function updateUiState() {
    const connectBtn = document.getElementById('btnConnect');
    const disconnectBtn = document.getElementById('btnDisconnect');
    const playBtn = document.getElementById('btnPlay');
    const sendBtn = document.getElementById('btnSend');
    const codecSelect = document.getElementById('codec');

    const hasPc = !!peerConnection;
    const hasStream = !!remoteVideo.srcObject;
    const canSend = !!(dataChannel && dataChannel.readyState === 'open');

    if (connectBtn) connectBtn.disabled = hasPc;
    if (disconnectBtn) disconnectBtn.disabled = !hasPc;
    if (playBtn) playBtn.disabled = !hasStream;
    if (sendBtn) sendBtn.disabled = !canSend;
    if (dataTextInput) dataTextInput.disabled = !canSend;
    if (codecSelect) codecSelect.disabled = hasPc; // The connection is not allowed to be changed during the connection
  }

  function prepareNewConnection() {
    // Reset the candidate buffer state for a new connection
    candidates = [];
    hasReceivedSdp = false;

    const peer = new RTCPeerConnection(peerConnectionConfig);
    dataChannel = peer.createDataChannel('serial');
    // Update the UI according to the DataChannel state
    dataChannel.onopen = () => { console.log('DataChannel open'); updateUiState(); };
    dataChannel.onclose = () => { console.log('DataChannel close'); updateUiState(); };

    if ('ontrack' in peer) {
      if (isSafari()) {
        const tracks = [];
        peer.ontrack = (event) => {
          console.log('-- peer.ontrack()');
          tracks.push(event.track);
          // Create a MediaStream every time ontrack is fired to make it work on safari
          const mediaStream = new MediaStream(tracks);
          playVideo(remoteVideo, mediaStream);
        };
      } else {
        const mediaStream = new MediaStream();
        playVideo(remoteVideo, mediaStream);
        peer.ontrack = (event) => {
          console.log('-- peer.ontrack()');
          mediaStream.addTrack(event.track);
        };
      }
    } else {
      peer.onaddstream = (event) => {
        console.log('-- peer.onaddstream()');
        playVideo(remoteVideo, event.stream);
      };
    }

    peer.onicecandidate = (event) => {
      console.log('-- peer.onicecandidate()');
      if (event.candidate) {
        console.log(event.candidate);
        sendIceCandidate(event.candidate);
      } else {
        console.log('empty ice event');
      }
    };

    peer.oniceconnectionstatechange = () => {
      console.log('-- peer.oniceconnectionstatechange()');
      console.log('ICE connection Status has changed to ' + peer.iceConnectionState);
      switch (peer.iceConnectionState) {
        case 'closed':
        case 'failed':
        case 'disconnected':
          break;
      }
      updateUiState();
    };

    const videoTransceiver = peer.addTransceiver('video', { direction: 'recvonly' });
    applyVideoCodecPreferences(videoTransceiver);
    peer.addTransceiver('audio', { direction: 'recvonly' });

    dataChannel.onmessage = (event) => {
      console.log('Got Data Channel Message:', new TextDecoder().decode(event.data));
    };

    return peer;
  }

  // Bind the UI events to the test page elements if they exist
  function bindUi() {
    const connectBtn = document.getElementById('btnConnect');
    const disconnectBtn = document.getElementById('btnDisconnect');
    const playBtn = document.getElementById('btnPlay');
    const sendBtn = document.getElementById('btnSend');
    if (connectBtn) connectBtn.addEventListener('click', () => connect());
    if (disconnectBtn) disconnectBtn.addEventListener('click', () => disconnect());
    if (playBtn) playBtn.addEventListener('click', () => play());
    if (sendBtn) sendBtn.addEventListener('click', () => sendDataChannel());
    if (dataTextInput) {
      dataTextInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') sendDataChannel();
      });
      // Immediately reflect the sendability of the input change
      dataTextInput.addEventListener('input', () => updateUiState());
    }
    // Reflect the initial state
    updateUiState();
  }

  // Helper functions related to codec preferences
  function getSelectedVideoCodecMime() {
    const el = document.getElementById('codec');
    if (!el) return null;
    return (el.value || '').toLowerCase() || null; // The value is a complete mimeType like 'video/h264'
  }

  function labelForMime(mime) {
    const lower = (mime || '').toLowerCase();
    const name = lower.split('/')[1] || lower;
    if (name === 'vp8') return 'VP8';
    if (name === 'vp9') return 'VP9';
    if (name === 'av1') return 'AV1';
    if (name === 'h264') return 'H264';
    if (name === 'h265' || name === 'hevc') return 'H265';
    return name.toUpperCase();
  }

  function isVideoPrimaryCodec(codec) {
    const mt = (codec && codec.mimeType ? codec.mimeType : '').toLowerCase();
    if (!mt.startsWith('video/')) return false;
    return !/(\/rtx|\/red|\/ulpfec|\/flexfec)/.test(mt);
  }

  function populateVideoCodecSelect() {
    const select = document.getElementById('codec');
    if (!select) return;
    if (typeof RTCRtpSender === 'undefined' || !RTCRtpSender.getCapabilities) return;
    const caps = RTCRtpSender.getCapabilities('video');
    if (!caps || !Array.isArray(caps.codecs)) return;

    const before = (select.value || '').toLowerCase();
    // Exclude duplicates by mimeType and exclude RTX/FEC systems
    const seen = new Set();
    const mimes = [];
    caps.codecs.forEach(c => {
      if (!isVideoPrimaryCodec(c)) return;
      const mt = (c.mimeType || '').toLowerCase();
      if (seen.has(mt)) return;
      seen.add(mt);
      mimes.push(mt);
    });

    // Sort by VP8/VP9/AV1/H264/H265
    const codecOrder = ['video/vp8', 'video/vp9', 'video/av1', 'video/h264', 'video/h265'];
    const sortedMimes = [];
    codecOrder.forEach(codec => {
      if (mimes.includes(codec)) {
        sortedMimes.push(codec);
      }
    });
    // Add the codecs other than the above to the end
    mimes.forEach(mt => {
      if (!sortedMimes.includes(mt)) {
        sortedMimes.push(mt);
      }
    });

    // Rebuild by first emptying it
    while (select.firstChild) select.removeChild(select.firstChild);
    sortedMimes.forEach(mt => {
      const opt = document.createElement('option');
      opt.value = mt;
      opt.textContent = labelForMime(mt);
      select.appendChild(opt);
    });
    // If there is a previous selection, maintain it, if not, H264, if not, the first
    const prefer = before && sortedMimes.includes(before) ? before : (sortedMimes.includes('video/h264') ? 'video/h264' : (sortedMimes[0] || ''));
    if (prefer) select.value = prefer;
  }

  function applyVideoCodecPreferences(transceiver) {
    const mime = getSelectedVideoCodecMime();
    if (!mime) return;
    if (typeof RTCRtpSender === 'undefined' || !RTCRtpSender.getCapabilities) return;
    if (!transceiver || typeof transceiver.setCodecPreferences !== 'function') return;
    const caps = RTCRtpSender.getCapabilities('video');
    if (!caps || !Array.isArray(caps.codecs)) return;
    // Exclude RTX/FEC systems and use only the primary video codec
    const primary = caps.codecs.filter(isVideoPrimaryCodec);
    const selected = primary.filter(c => (c.mimeType || '').toLowerCase() === mime);
    const others = primary.filter(c => (c.mimeType || '').toLowerCase() !== mime);
    if (selected.length === 0) return; // The selected codec is not supported
    const ordered = selected.concat(others);
    transceiver.setCodecPreferences(ordered);
    console.log('Applied codec preferences via setCodecPreferences:', mime);
  }

  function browser() {
    const ua = window.navigator.userAgent.toLocaleLowerCase();
    if (ua.indexOf('edge') !== -1) return 'edge';
    if (ua.indexOf('chrome') !== -1 && ua.indexOf('edge') === -1) return 'chrome';
    if (ua.indexOf('safari') !== -1 && ua.indexOf('chrome') === -1) return 'safari';
    if (ua.indexOf('opera') !== -1) return 'opera';
    if (ua.indexOf('firefox') !== -1) return 'firefox';
    return undefined;
  }

  function isSafari() {
    return browser() === 'safari';
  }

  function sendSdp(sessionDescription) {
    console.log('---sending sdp ---');
    const message = JSON.stringify(sessionDescription);
    console.log('sending SDP=' + message);
    ws.send(message);
  }

  async function makeOffer() {
    peerConnection = prepareNewConnection();
    updateUiState();
    try {
      const sessionDescription = await peerConnection.createOffer({
        // The reception direction is specified by addTransceiver('recvonly')
      });
      console.log('createOffer() success in promise, SDP=', sessionDescription.sdp);
      await peerConnection.setLocalDescription(sessionDescription);
      console.log('setLocalDescription() success in promise');
      sendSdp(peerConnection.localDescription);
    } catch (error) {
      console.error('makeOffer() ERROR:', error);
    }
  }

  async function makeAnswer() {
    console.log('sending Answer. Creating remote session description...');
    if (!peerConnection) {
      console.error('peerConnection DOES NOT exist!');
      return;
    }
    try {
      const sessionDescription = await peerConnection.createAnswer();
      console.log('createAnswer() success in promise');
      await peerConnection.setLocalDescription(sessionDescription);
      console.log('setLocalDescription() success in promise');
      sendSdp(peerConnection.localDescription);
      drainCandidate();
    } catch (error) {
      console.error('makeAnswer() ERROR:', error);
    }
  }

  // Processing when offer SDP is received
  async function setOffer(sessionDescription) {
    if (peerConnection) {
      console.warn('peerConnection already exists. Replacing with a new one.');
      try { peerConnection.close(); } catch (_) {}
    }
    peerConnection = prepareNewConnection();
    updateUiState();
    try {
      await peerConnection.setRemoteDescription(sessionDescription);
      console.log('setRemoteDescription(offer) success in promise');
      await makeAnswer();
    } catch (error) {
      console.error('setRemoteDescription(offer) ERROR:', error);
    }
  }

  async function setAnswer(sessionDescription) {
    if (!peerConnection) {
      console.error('peerConnection DOES NOT exist!');
      return;
    }
    try {
      await peerConnection.setRemoteDescription(sessionDescription);
      console.log('setRemoteDescription(answer) success in promise');
      drainCandidate();
    } catch (error) {
      console.error('setRemoteDescription(answer) ERROR:', error);
    }
  }

  function cleanupVideoElement(element) {
    try {
      const stream = element.srcObject;
      if (stream && typeof stream.getTracks === 'function') {
        stream.getTracks().forEach((t) => {
          try { t.stop(); } catch (_) {}
        });
      }
    } catch (e) {
      console.warn('cleanupVideoElement: stop tracks error:', e);
    }
    element.pause();
    element.srcObject = null;
    try { updateUiState(); } catch (_) {}
  }

  // SDP replacement is discontinued and unified only with setCodecPreferences

  // Global API (no class used)
  window.connect = connect;
  window.disconnect = disconnect;
  window.play = play;
  window.sendDataChannel = sendDataChannel;
})();
