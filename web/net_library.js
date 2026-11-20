addToLibrary({
  $EvoNet: {
    peer: null,
    conns: {},
    nextId: 1,
    queue: [],
    status: 'Idle',
    ready: false,
    prefix: 'evo-tourney-v1-',
    push: function (type, id, bytes) {
      EvoNet.queue.push({ type: type, id: id, bytes: bytes || null });
    },
    toBytes: function (data) {
      if (data instanceof ArrayBuffer) {
        return new Uint8Array(data);
      }
      if (ArrayBuffer.isView(data)) {
        return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
      }
      return null;
    },
    drop: function (id) {
      if (EvoNet.conns[id]) {
        delete EvoNet.conns[id];
        EvoNet.push(1, id);
      }
    },
    attach: function (conn, id) {
      EvoNet.conns[id] = conn;
      conn.on('open', function () {
        EvoNet.ready = true;
        EvoNet.push(0, id);
      });
      conn.on('data', function (data) {
        var bytes = EvoNet.toBytes(data);
        if (bytes) {
          EvoNet.push(2, id, bytes.slice());
        }
      });
      conn.on('close', function () { EvoNet.drop(id); });
      conn.on('error', function () { EvoNet.drop(id); });
    },
    reset: function () {
      try {
        for (var id in EvoNet.conns) {
          EvoNet.conns[id].close();
        }
        if (EvoNet.peer) {
          EvoNet.peer.destroy();
        }
      } catch (e) {
      }
      EvoNet.peer = null;
      EvoNet.conns = {};
      EvoNet.queue = [];
      EvoNet.nextId = 1;
      EvoNet.ready = false;
      EvoNet.status = 'Idle';
    },
    available: function () {
      if (typeof Peer === 'undefined') {
        EvoNet.status = 'Networking library failed to load (check your connection)';
        return false;
      }
      return true;
    }
  },

  evo_net_host__deps: ['$EvoNet'],
  evo_net_host: function (roomPtr) {
    EvoNet.reset();
    if (!EvoNet.available()) {
      return 0;
    }
    var room = UTF8ToString(roomPtr).toUpperCase();
    EvoNet.status = 'Opening room ' + room + '...';
    var peer = new Peer(EvoNet.prefix + room, { debug: 0 });
    EvoNet.peer = peer;
    peer.on('open', function () {
      EvoNet.ready = true;
      EvoNet.status = 'Room ' + room + ' is open';
    });
    peer.on('connection', function (conn) {
      EvoNet.attach(conn, EvoNet.nextId++);
    });
    peer.on('disconnected', function () {
      if (EvoNet.peer === peer && !peer.destroyed) {
        try { peer.reconnect(); } catch (e) {}
      }
    });
    peer.on('error', function (err) {
      if (err.type === 'unavailable-id') {
        EvoNet.status = 'Room code ' + room + ' is already in use';
      } else if (err.type !== 'peer-unavailable') {
        EvoNet.status = 'Network error: ' + err.type;
      }
    });
    return 1;
  },

  evo_net_join__deps: ['$EvoNet'],
  evo_net_join: function (roomPtr) {
    EvoNet.reset();
    if (!EvoNet.available()) {
      return 0;
    }
    var room = UTF8ToString(roomPtr).toUpperCase().trim();
    EvoNet.status = 'Contacting matchmaking server...';
    var peer = new Peer({ debug: 0 });
    EvoNet.peer = peer;
    peer.on('open', function () {
      EvoNet.status = 'Connecting to room ' + room + '...';
      var conn = peer.connect(EvoNet.prefix + room, { reliable: false });
      EvoNet.attach(conn, 0);
    });
    peer.on('error', function (err) {
      EvoNet.status = err.type === 'peer-unavailable'
        ? 'Room ' + room + ' was not found'
        : 'Network error: ' + err.type;
      if (EvoNet.conns[0]) {
        EvoNet.drop(0);
      } else {
        EvoNet.push(1, 0);
      }
    });
    return 1;
  },

  evo_net_send__deps: ['$EvoNet'],
  evo_net_send: function (id, ptr, length) {
    var conn = EvoNet.conns[id];
    if (!conn || !conn.open) {
      return;
    }
    try {
      conn.send(HEAPU8.slice(ptr, ptr + length).buffer);
    } catch (e) {
    }
  },

  evo_net_poll__deps: ['$EvoNet'],
  evo_net_poll: function (buffer, capacity, peerPtr, typePtr) {
    var event = EvoNet.queue.shift();
    if (!event) {
      return -1;
    }
    var length = 0;
    if (event.bytes) {
      length = Math.min(capacity, event.bytes.length);
      HEAPU8.set(event.bytes.subarray(0, length), buffer);
    }
    HEAP32[peerPtr >> 2] = event.id;
    HEAP32[typePtr >> 2] = event.type;
    return length;
  },

  evo_net_status__deps: ['$EvoNet'],
  evo_net_status: function (buffer, capacity) {
    stringToUTF8(EvoNet.status, buffer, capacity);
  },

  evo_net_ready__deps: ['$EvoNet'],
  evo_net_ready: function () {
    return EvoNet.ready ? 1 : 0;
  },

  evo_net_shutdown__deps: ['$EvoNet'],
  evo_net_shutdown: function () {
    EvoNet.reset();
  }
});
