(function ($) {

Drupal.behaviors.ajax_table = {
  attach: function (context, settings) {
    // If there is AJAX on the page
    if (Drupal.ajax != undefined) {
      Drupal.ajax.prototype.commands.proper_replace = function (ajax, response, status) {
        $(response.selector).replaceWith(response.data);
        //Drupal.attachBehaviors(response.selector);
        Drupal.attachBehaviors();      
      }
    }
    // Graceful degradation
    $('.ajax-table-table-wrapper a.ajax-table-link')
      .add('.ajax-table-pager-wrapper a.ajax-table-link')
      .each(function() {
        if ($(this).hasClass('ajax-table-ajax-link')) $(this).show();
        if ($(this).hasClass('ajax-table-non-js-link')) $(this).hide();
      }); 
  }
};

Drupal.behaviors.ajax_table_groups = {
  attach: function (context, settings) {

    if (typeof Drupal.settings == 'undefined' ||
      typeof Drupal.settings.ajax_table_groups == 'undefined'
    ) {
      return;
    }
    
    /**
     * Find parent of row
     */
    Drupal.tableDrag.prototype.row.prototype.findParent = function (indentAlter) {
      var parentIndentation = this.indents;
      if (typeof indent != 'undefined' ) parentIndentation = indentAlter;
      return $(this.element, this.table)
        .prevAll('tr.draggable')
        .filter(function () {
          if ($('.indentation', this).length === parentIndentation - 1) {
            return true;
          }
          return false;
        })
        .first();
    };
    
    /**
     * Find parent of row
     */
    Drupal.tableDrag.prototype.row.prototype.haveChildren = function () {
      return $(this.element, this.table)
        .next('tr.draggable')
        .find('.indentation')
        .length > this.indents;
    };    
    
    Drupal.tableDrag.prototype.row.prototype.indentCalc = function (indentDiff) {
      // Determine the valid indentations interval if not available yet.
      if (!this.interval) {
        var prevRow = $(this.element).prev('tr').get(0);
        var nextRow = $(this.group).filter(':last').next('tr').get(0);
        this.interval = this.validIndentInterval(prevRow, nextRow);
      }

      // Adjust to the nearest valid indentation.
      var indent = this.indents + indentDiff;
      indent = Math.max(indent, this.interval.min);
      indent = Math.min(indent, this.interval.max);
      indentDiff = indent - this.indents;
      
      var indents = this.indents;

      for (var n = 1; n <= Math.abs(indentDiff); n++) {
        if (indentDiff < 0) indents--;
        else indents++;
      }

      return indents;
    };
    
    /**
     * http://stackoverflow.com/questions/281264/remove-empty-elements-from-an-array-in-javascript
     */
    function cleanArray(array, deleteValue) {
      for (var i = 0; i < array.length; i++) {
        if (array[i] == deleteValue) {         
          array.splice(i, 1);
          i--;
        }
      }
      return array;
    };
    
    /**
     * http://stackoverflow.com/questions/3504795/how-do-i-check-with-jquery-if-an-element-has-at-least-one-class-from-a-list-of
     */
    $.fn.hasClassMultiple = function (classNames) {
      if (!classNames || typeof classNames === "string" ) {
        return $(this).hasClass(classNames); // Default behavior
      } else {
        // Take array and parse it for the filter method
        var names = classNames.slice();
        var classes = cleanArray(names, null).join(', .');
        if (classes.length > 0) classes = '.' + classes;
        return this.filter(classes).length > 0;
      }
    };
    
    /**
     * Override Drupal.tableDrag.prototype.dragRow
     */
    Drupal.tableDrag.prototype.dragRow = function (event, self) {
      if (self.dragObject) {
      
        var useGroups = false;
        var id;
        var groups;
        
        for (var i in Drupal.settings.ajax_table_groups) {
          id = Drupal.settings.ajax_table_groups[i]['id'];
          groups = Drupal.settings.ajax_table_groups[i]['groups'];
          
          if (typeof Drupal.tableDrag == 'undefined' ||
            typeof Drupal.tableDrag[id] == 'undefined' ||
            typeof Drupal.tableDrag[id].table == 'undefined' ||
            Drupal.tableDrag[id].table != self.table
          ) {
            continue;
          }
          useGroups = true;
        }

        var rowObject = $(self.rowObject.element);
        useGroups = false;
        var group;
        var groupChild;
        for (var childClass in groups) {
          if (rowObject.hasClassMultiple(childClass)) {
            group = groups[childClass];
            groupChild = childClass;
            useGroups = group.length > 0;
            break;
          }
        }

        self.currentMouseCoords = self.mouseCoords(event);

        var y = self.currentMouseCoords.y - self.dragObject.initMouseOffset.y;
        var x = self.currentMouseCoords.x - self.dragObject.initMouseOffset.x;

        // Check for row swapping and vertical scrolling.
        if (y != self.oldY) {
          self.rowObject.direction = y > self.oldY ? 'down' : 'up';
          self.oldY = y; // Update the old value.

          // Check if the window should be scrolled (and how fast).
          var scrollAmount = self.checkScroll(self.currentMouseCoords.y);
          // Stop any current scrolling.
          clearInterval(self.scrollInterval);
          // Continue scrolling if the mouse has moved in the scroll direction.
          if (scrollAmount > 0 && self.rowObject.direction == 'down' || scrollAmount < 0 && self.rowObject.direction == 'up') {
            self.setScroll(scrollAmount);
          }

          // If we have a valid target, perform the swap and restripe the table.
          var currentRow = self.findDropTargetRow(x, y);
          if (currentRow) {
            var currentRow = new self.row(currentRow, 'code', self.indentEnabled, self.maxDepth, false);
            var currentRowParent = currentRow.findParent();             
            if (
              // if no groups used - normal behavior
              useGroups == false ||
              ((
                // if null if in group and no parent - root assumed
                (group.every(function(v) { return v !== null; }) == false &&
                  currentRowParent.length == 0) ||
                // if parent and has class - normal restriction
                currentRowParent.hasClassMultiple(group) ||
                // if sibling and has class and child class == parent class
                // this is additional functionality - root sections support  
                ($.inArray(groupChild, group) != -1 &&
                  $(self.rowObject.element).hasClass(groupChild) && 
                  $(currentRow.element).hasClass(groupChild))
              ) &&
              // move restrictions - do not move down if have children
              // TODO and children is not last
              (self.rowObject.direction != 'down' ||
                (currentRow.haveChildren() == false)))
            ) {
              if (self.rowObject.direction == 'down') {
                self.rowObject.swap('after', currentRow.element, self);
              }
              else {
                self.rowObject.swap('before', currentRow.element, self);
              }
              self.restripeTable();         
            }
          }
        }

        // Similar to row swapping, handle indentations.
        if (self.indentEnabled && false) {
          var xDiff = self.currentMouseCoords.x - self.dragObject.indentMousePos.x;
          // Set the number of indentations the mouse has been moved left or right.
          var indentDiff = Math.round(xDiff / self.indentAmount * self.rtl);

          var indentChange = self.rowObject.indentCalc(indentDiff);
          if (useGroups == false ||
            (group.every(function(v) { return v !== null; }) == false &&
              self.rowObject.findParent(indentChange).length == 0) ||
            self.rowObject.findParent(indentChange).hasClassMultiple(group)
          ) {
            // Indent the row with our estimated diff, which may be further
            // restricted according to the rows around this row.             
            indentChange = self.rowObject.indent(indentDiff);

            // Update table and mouse indentations.
            self.dragObject.indentMousePos.x += self.indentAmount * indentChange * self.rtl;
            self.indentCount = Math.max(self.indentCount, self.rowObject.indents);   
          }
        }

        return false;
      }
    };
  }
};

})(jQuery);