%
% Authors:
%   Andreas Simon (2000)
%
% Copyright:
%   Andreas Simon (2000)
%
% Last change:
%   $Date$
%   $Revision$
%
% This file is part of Mozart, an implementation
% of Oz 3:
%   http://www.mozart-oz.org
%
% See the file "LICENSE" or
%   http://www.mozart-oz.org/LICENSE.html
% for information on usage and redistribution
% of this file, and for a DISCLAIMER OF ALL
% WARRANTIES.
%

functor

import
   Native at 'GTK.so{native}'
   System
   GTKSimpleBuilder(make:Make)

export
   Dispatcher

   % Registry stuff
   GetObject
   RegisterNativeObject
   UnregisterNativeObject
   RegisterObject

   % Non-autogenerated GTK classes
   Object

   % Misc
   Exit
   Main
   MainQuit
   Make

   \insert 'gtkexports.oz'

define

% -----------------------------------------------------------------------------
% Object Registry
% -----------------------------------------------------------------------------

   % stores GTK object --> OZ object corrospondence
   ObjectRegistry = {Dictionary.new $}

   % store a GTK object --> Oz object corrospondence
   proc {RegisterObject Object}
      NativeObj = {Object getNative($)}
   in
      {Dictionary.put ObjectRegistry {ForeignPointer.toInt NativeObj} Object}
   end

   % store a GTK object --> Oz object corrospondence
   proc {RegisterNativeObject Object NativeObj}
      {Dictionary.put ObjectRegistry {ForeignPointer.toInt NativeObj} Object}
   end

   proc {UnregisterNativeObject NativeObj}
      {Dictionary.remove ObjectRegistry {ForeignPointer.toInt NativeObj}}
   end

   % Get the corrosponding Oz object from a GTK object
   proc {GetObject MyForeignPointer ?MyObject}
      if MyForeignPointer == 0 then
         MyObject = nil
      else
         {Dictionary.get
          ObjectRegistry
          {ForeignPointer.toInt MyForeignPointer}
          MyObject}
      end
   end

% -----------------------------------------------------------------------------
% Object
% -----------------------------------------------------------------------------

   class Object
      attr nativeObject

      % For building Oz object from native objects
      % Only for internal use!
      meth newWrapper(NativeObj)
         nativeObject <- NativeObj
      end
      meth getNative($) % get native GTK object from an Oz object
         @nativeObject
      end

      meth ref
         {Native.ref @nativeObject}
      end
      meth unref
         {Native.unref @nativeObject}
      end

% Signal (made part of Object)

      meth signalConnect(Name Handler ?Id)
         % TODO: support user data (maybe superfluous)
         {Dispatcher registerHandler(Handler Id)}
         {Native.signalConnect @nativeObject Name Id _}
      end
      meth signalDisconnect(Id)
         {Dispatcher unregisterSignal(Id)}
         {Native.signalDisconnect @nativeObject Id}
      end
      meth signalHandlerBlock(HandlerId)
         {Native.signalBlock @nativeObject HandlerId}
      end
      meth signalHandlerUnblock(HandlerId)
         {Native.signalUnblock @nativeObject HandlerId}
      end
      meth signalEmitByName(Name)
         {Native.signalEmitByName @nativeObject Name}
      end
   end

% -----------------------------------------------------------------------------
% Dispatcher
% -----------------------------------------------------------------------------

   local
      PollingIntervall = 50
   in
      class DispatcherClass
         attr
            id_counter : 0
            registry % A dictionary with id <--> handler corrospondences
            port
            stream
            fillerThread
         meth init
            proc {FillStream}
               {Native.handlePendingEvents} % Send all pending GTK events to the Oz port
               {Time.delay PollingIntervall}
               {FillStream}
            end
         in
            registry <- {Dictionary.new}
            port     <- {Port.new @stream}
            {Native.initializeSignalPort @port} % Tell the 'C side' about the signal port
            thread
               fillerThread <- {Thread.this $}
               {FillStream}
            end
         end
         meth GetUniqueID($)
            id_counter <- @id_counter + 1
            @id_counter
         end
         meth registerHandler(Handler ?Id)
         {self GetUniqueID(Id)}
            {Dictionary.put @registry Id Handler}
         end
         meth unregisterHandler(Id)
            {Dictionary.remove @registry Id}
         end
         meth dispatch
            Handler
            Event
            Tail
         in
            @stream = Event|Tail
            {Dictionary.get @registry Event Handler}
            % TODO: suspend marshaller with sending a new variable to a second port
            {Handler}
            % TODO: terminate marshaller with bounding this variable
            stream <- Tail
            DispatcherClass,dispatch
         end
         meth exit
            {Thread.terminate {Thread.this $}}
         end
      end
   end

% -----------------------------------------------------------------------------
% The abandoned and homeless
% -----------------------------------------------------------------------------

   proc {Main}
      {Native.main}
   end

   proc {MainQuit}
      {Dispatcher exit}
      {Native.mainQuit}
   end

   proc {Exit}
      {Native.exit 0}
      {Dispatcher exit}
   end

% -----------------------------------------------------------------------------
% Autogenerated stuff
% -----------------------------------------------------------------------------

   local
      NULL = {Native.getNull}
   in
      fun {GetNativeOrUnit X}
         if X==unit then NULL else {X getNative($)} end
      end
   end

   \insert 'gtkclasses.oz'

% -----------------------------------------------------------------------------
% Finale
% -----------------------------------------------------------------------------

   % Start the dispatcher
   Dispatcher = {New DispatcherClass init}
   thread {Dispatcher dispatch} end
   {System.show 'Dispatcher successfully started and running ...'}

end % functor
