var Sol = Sol || {};

Sol.Multirange = function (options) {
  this.o = {
    item_size:     3,
    item_spacing:  3,
    item_count:    100,
    index_size:    8,
    gap:           1,
    border:        4,
    marker_height: 4,
  };

  for (var key in options) { this.o[key] = options[key] };

  this.detent = this.o.item_size + this.o.item_spacing;

  this.stage = new Konva.Stage({
    container: this.o.container,
    width: this.o.border * 2 + this.o.index_size * 2 + this.detent * this.o.item_count - this.o.item_spacing,
    height: this.o.border * 2 + this.o.marker_height + this.o.gap + this.o.item_size + this.o.gap + this.o.index_size
  });

  this.layer = new Konva.Layer();
  this.stage.add(this.layer);

  this.items = [];
  for (i = 0; i < this.o.item_count; i++) {
    var item = new Konva.Rect({
      width: this.o.item_size,
      height: this.o.item_size,
      fill: 'black',
      x: this.o.border + this.o.index_size + this.detent * i,
      y: this.o.border + this.o.marker_height + this.o.gap
    });
    this.layer.add(item);
    this.items[i] = item;
  }  

  this.markers = new Konva.Group({
    x: this.o.border + this.o.index_size,
    y: this.o.border,
  });
  this.layer.add(this.markers);

  this.hotspots = new Konva.Group({
    x: this.o.border + this.o.index_size,
    y: this.o.border,
  });
  this.layer.add(this.hotspots);

  this.hotspots.on('mouseover', function() {
      document.body.style.cursor = 'pointer';
  });
  this.hotspots.on('mouseout', function() {
      document.body.style.cursor = 'default';
  });

  var t = this;

  this.lbindex = new Konva.Group({
    visible: false,
    draggable: true,
    dragBoundFunc: function(pos) {
      var lb = Math.max(t.min, Math.min(t.ub, Math.round((pos.x - t.o.border) / t.detent)));
      if (t.lb != lb) {
        t.lb = lb;
        if (typeof t.o.change_func == 'function') {
          t.o.change_func('select', {lb: t.lb, ub: t.ub});
        }
      }
      return {
        x: t.o.border + t.detent * t.lb,
        y: this.getAbsolutePosition().y
      }
    },
    y: this.o.border + this.o.marker_height + this.o.gap + this.o.item_size + this.o.gap
  });
  this.lbindex.add(new Konva.Rect({
    width: this.o.index_size,
    height: this.o.index_size,
  }));
  this.lbindex.add(new Konva.Line({
    points: [this.o.index_size, 0, 0, this.o.index_size, this.o.index_size, this.o.index_size],
    fill: 'black',
    closed : true
  }));
  this.layer.add(this.lbindex);
  this.lbindex.on('mouseover', function() {
      document.body.style.cursor = 'pointer';
  });
  this.lbindex.on('mouseout', function() {
      document.body.style.cursor = 'default';
  });

  this.ubindex = new Konva.Group({
    visible: false,
    draggable: true,
    dragBoundFunc: function(pos) {
      var ub = Math.max(t.lb, Math.min(t.max, Math.round((pos.x - t.o.border - t.o.index_size - t.o.item_size) / t.detent)));
      if (t.ub != ub) {
        t.ub = ub;
        if (typeof t.o.change_func == 'function') {
          t.o.change_func('select', {lb: t.lb, ub: t.ub});
        }
      }
      return {
        x: t.o.border + t.o.index_size + t.o.item_size + t.detent * t.ub,
        y: this.getAbsolutePosition().y
      }
    },
    y: this.o.border + this.o.marker_height + this.o.gap + this.o.item_size + this.o.gap
  });
  this.ubindex.add(new Konva.Rect({
    width: this.o.index_size,
    height: this.o.index_size,
  }));
  this.ubindex.add(new Konva.Line({
    points: [0, 0, this.o.index_size, this.o.index_size, 0, this.o.index_size],
    fill: 'black',
    closed : true
  }));
  this.layer.add(this.ubindex);

  this.ubindex.on('mouseover', function() {
      document.body.style.cursor = 'pointer';
  });
  this.ubindex.on('mouseout', function() {
      document.body.style.cursor = 'default';
  });

  this.ranges = [];
  this._update_hotspots();

  this.active = false;
  this.editing = -1;

  this.layer.draw();
}

Sol.Multirange.prototype._update_hotspots = function() {
  this.hotspots.destroyChildren();

  var t = this;
  var last_ub = -1;

  for (var i = 0; i < this.ranges.length; i++) {
    var range = this.ranges[i];

    if (last_ub + 1 < range.lb) {
      var rect = new Konva.Rect({
        x: this.detent * (last_ub + 1) - (this.o.item_spacing / 2),
        width: this.detent * (range.lb - last_ub - 1),
        height: this.o.marker_height + this.o.gap + this.o.item_size + this.o.gap,
      });
      this.hotspots.add(rect);

      rect.on('click tap', (function (min, max) {
        return function() {
          if (!t.active || t.editing != -1) {
            t.editing = -1;
            t.active = true;
            if (typeof t.o.change_func == 'function') {
              t.o.change_func('edit', t.editing);
            }
          }

          t.min = min;
          t.max = max;
          t._select({lb: min, ub: max});
        }
      })(last_ub + 1, range.lb - 1));
    }

    var rect = new Konva.Rect({
      x: this.detent * range.lb - (this.o.item_spacing / 2),
      width: this.detent * (range.ub - range.lb + 1),
      height: this.o.marker_height + this.o.gap + this.o.item_size + this.o.gap,
    });
    this.hotspots.add(rect);

    rect.on('click tap', (function (i) {
      return function() {
        t.range_select(i)
      }
    })(i));

    last_ub = range.ub;
  }

  if (last_ub < this.o.item_count - 1) {
    var rect = new Konva.Rect({
      x: this.detent * (last_ub + 1) - (this.o.item_spacing / 2),
      width: this.detent * (this.o.item_count - last_ub - 1),
      height: this.o.marker_height + this.o.gap + this.o.item_size + this.o.gap,
    });
    this.hotspots.add(rect);

    rect.on('click tap', (function (min, max) {
      return function() {
        if (!t.active || t.editing != -1) {
          t.editing = -1;
          t.active = true;
          if (typeof t.o.change_func == 'function') {
            t.o.change_func('edit', -1);
          }
        }

        t.min = min;
        t.max = max;
        t._select({lb: min, ub: max});
      }
    })(last_ub + 1, t.o.item_count - 1));
  }
}

Sol.Multirange.prototype._select = function(range) {
  this.lbindex.visible(true);
  this.ubindex.visible(true);

  this.lb = range.lb;
  this.ub = range.ub;

  this.lbindex.position({x: this.o.border + this.detent * this.lb});
  this.ubindex.position({x: this.o.border + this.o.index_size + this.o.item_size + this.detent * this.ub});

  if (typeof this.o.change_func == 'function') {
    this.o.change_func('select', {lb: this.lb, ub: this.ub});
  }

  this.layer.draw();
}

Sol.Multirange.prototype.range_add = function(range, deselect) {
  if (typeof deselect == 'undefined') {
    var deselect = true;
  }

  range.lb = Math.min(this.o.item_count - 1, Math.max(0, range.lb));
  range.ub = Math.min(this.o.item_count - 1, Math.max(0, range.ub));
  if (isNaN(range.lb) || isNaN(range.ub) || range.lb > range.ub) {
    return -1;
  }

  for (var i = 0; i < this.ranges.length; i++) {
    if ((range.lb >= this.ranges[i].lb && range.lb <= this.ranges[i].ub) ||
        (range.ub >= this.ranges[i].lb && range.ub <= this.ranges[i].ub)) {
      return -1;
    }
  }

  for (var i = 0; i <= this.ranges.length; i++) {
    if (i == this.ranges.length || range.lb < this.ranges[i].lb) {
      this.ranges.splice(i, 0, range);
      break;
    }
  }

  if (deselect && this.active) {
    this.active = false;
    if (typeof this.o.change_func == 'function') {
      this.o.change_func('deselect', null);
    }
    this.lbindex.visible(false);
    this.ubindex.visible(false);
  }

  range.marker = new Konva.Line({
    points: [this.detent * range.lb + 0.5, this.o.marker_height, this.detent * range.lb + 0.5, 0.5, this.detent * range.ub + this.o.item_size - 0.5, 0.5, this.detent * range.ub + this.o.item_size - 0.5, this.o.marker_height],
    stroke: 'black',
    strokeWidth: 1,
  })
  this.markers.add(range.marker);

  this._update_hotspots();

  this.layer.draw();

  return i;
}

Sol.Multirange.prototype.range_update = function(i, range) {
  range.lb = Math.min(this.o.item_count - 1, Math.max(0, range.lb));
  range.ub = Math.min(this.o.item_count - 1, Math.max(0, range.ub));
  if (isNaN(range.lb) || isNaN(range.ub) || range.lb > range.ub) {
    return false;
  }

  for (var j = 0; j < this.ranges.length; j++) {
    if (i != j) {
      if ((range.lb >= this.ranges[j].lb && range.lb <= this.ranges[j].ub) ||
          (range.ub >= this.ranges[j].lb && range.ub <= this.ranges[j].ub)) {
        return false;
      }
    }
  }

  this.ranges[i].marker.destroy();

  this.ranges[i] = range;

  range.marker = new Konva.Line({
    points: [this.detent * range.lb + 0.5, this.o.marker_height, this.detent * range.lb + 0.5, 0.5, this.detent * range.ub + this.o.item_size - 0.5, 0.5, this.detent * range.ub + this.o.item_size - 0.5, this.o.marker_height],
    stroke: 'black',
    strokeWidth: 1,
  })
  this.markers.add(range.marker);

  this._update_hotspots();

  this.layer.draw();

  return true;
}

Sol.Multirange.prototype.range_remove = function(i) {
  var range = this.ranges[i];
  if (typeof range === 'undefined') {
    return false;
  }

  if (this.active) {
    this.active = false;
    if (typeof this.o.change_func == 'function') {
      this.o.change_func('deselect', null);
    }
  }

  this.lbindex.visible(false);
  this.ubindex.visible(false);

  this.ranges.splice(i, 1);
  range.marker.destroy();

  this._update_hotspots();

  this.layer.draw();

  return true;
}

Sol.Multirange.prototype.range_select = function(i) {
  var range = this.ranges[i];
  if (typeof range === 'undefined') {
    return;
  }

  if (i == 0) {
    var min = 0;
  }
  else {
    var min = this.ranges[i - 1].ub + 1;
  }
  if (i == this.ranges.length - 1) {
    var max = this.o.item_count - 1;
  }
  else {
    var max = this.ranges[i + 1].lb - 1;
  }

  this.editing = i;
  this.active = true;
  if (typeof this.o.change_func == 'function') {
    this.o.change_func('edit', this.editing);
  }

  this.min = min;
  this.max = max;
  this._select({lb: range.lb, ub: range.ub});
}

Sol.Multirange.prototype.add = function() {
  if (!this.active || this.editing != -1) {
    return;
  }
  
  var i = this.range_add({lb: this.lb, ub: this.ub}, false);
  if (i == -1) {
    return;
  }

  if (typeof this.o.change_func == 'function') {
    this.o.change_func('add', {i: i, lb: this.lb, ub: this.ub});
  }

  this.editing = i;
}

Sol.Multirange.prototype.update = function() {
  if (!this.active || this.editing == -1) {
    return;
  }

  var i = this.editing;
  if (!this.range_update(i, {lb: this.lb, ub: this.ub})) {
    return;
  }

  if (typeof this.o.change_func == 'function') {
    this.o.change_func('update', {i: i, lb: this.lb, ub: this.ub});
  }
}

Sol.Multirange.prototype.remove = function() {
  if (!this.active || this.editing == -1) {
    return;
  }

  var i = this.editing;
  if (!this.range_remove(i)) {
    return;
  }

  if (typeof this.o.change_func == 'function') {
    this.o.change_func('remove', i);
  }
}

Sol.Multirange.prototype.destroy = function() {
  this.stage.destroy();
}
