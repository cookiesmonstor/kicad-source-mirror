/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 Roberto Fernandez Bautista <@Qbort>
 * Copyright (C) 2020 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file cadstar_pcb.cpp
 * @brief Converts a CADSTAR_PCB_ARCHIVE_PARSER object into a KiCad BOARD object
 */

#include <cadstar_pcb.h>

#include <board_stackup_manager/stackup_predefined_prms.h> //KEY_COPPER, KEY_CORE, KEY_PREPREG
#include <class_drawsegment.h>                             // DRAWSEGMENT
#include <limits>                                          // std::numeric_limits
#include <trigo.h>


void CADSTAR_PCB::Load( ::BOARD* aBoard )
{
    mBoard = aBoard;
    Parse();

    wxPoint designSize =
            Assignments.Technology.DesignArea.first - Assignments.Technology.DesignArea.second;
    long long designSizeXkicad   = (long long) designSize.x * KiCadUnitMultiplier;
    long long designSizeYkicad   = (long long) designSize.y * KiCadUnitMultiplier;
    long long maxDesignSizekicad = (long long) std::numeric_limits<int>::max()
                                   + std::abs( std::numeric_limits<int>::min() );

    if( designSizeXkicad > maxDesignSizekicad || designSizeYkicad > maxDesignSizekicad )
        THROW_IO_ERROR( wxString::Format(
                _( "The design is too large and cannot be imported into KiCad. \n"
                   "Please reduce the maximum design size in CADSTAR by navigating to: \n"
                   "Design Tab -> Properties -> Design Options -> Maximum Design Size. \n"
                   "Current Design size: %d, %d micrometers. \n"
                   "Maximum permitted design size: %d, %d micrometers.\n" ),
                designSizeXkicad / 1000, designSizeYkicad / 1000, maxDesignSizekicad / 1000,
                maxDesignSizekicad / 1000 ) );

    mDesignCenter =
            ( Assignments.Technology.DesignArea.first + Assignments.Technology.DesignArea.second )
            / 2;


    loadBoardStackup();
    loadBoards();
    //TODO: process all other items
}


void CADSTAR_PCB::loadBoardStackup()
{
    std::map<LAYER_ID, LAYER>&       cpaLayers              = Assignments.Layerdefs.Layers;
    std::map<MATERIAL_ID, MATERIAL>& cpaMaterials           = Assignments.Layerdefs.Materials;
    std::vector<LAYER_ID>&           cpaLayerStack          = Assignments.Layerdefs.LayerStack;
    unsigned                         numElecAndPowerLayers  = 0;
    BOARD_DESIGN_SETTINGS&           designSettings         = mBoard->GetDesignSettings();
    BOARD_STACKUP&                   stackup                = designSettings.GetStackupDescriptor();
    int                              noOfKiCadStackupLayers = 0;
    int                              lastElectricalLayerIndex = 0;
    int                              dielectricSublayer       = 0;
    int                              numDielectricLayers      = 0;
    bool                             prevWasDielectric        = false;
    BOARD_STACKUP_ITEM*              tempKiCadLayer;
    std::vector<PCB_LAYER_ID>        layerIDs;

    //Remove all layers except required ones
    stackup.RemoveAll();
    layerIDs.push_back( PCB_LAYER_ID::F_CrtYd );
    layerIDs.push_back( PCB_LAYER_ID::B_CrtYd );
    layerIDs.push_back( PCB_LAYER_ID::Margin );
    layerIDs.push_back( PCB_LAYER_ID::Edge_Cuts );
    designSettings.SetEnabledLayers( LSET( &layerIDs[0], layerIDs.size() ) );

    for( auto it = cpaLayerStack.begin(); it != cpaLayerStack.end(); ++it )
    {
        LAYER                   curLayer       = cpaLayers[*it];
        BOARD_STACKUP_ITEM_TYPE kicadLayerType = BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_UNDEFINED;
        LAYER_T                 copperType     = LAYER_T::LT_UNDEFINED;
        PCB_LAYER_ID            kicadLayerID   = PCB_LAYER_ID::UNDEFINED_LAYER;
        wxString                layerTypeName  = wxEmptyString;

        if( cpaLayers.count( *it ) == 0 )
            wxASSERT_MSG( true, wxT( "Unable to find layer index" ) );

        if( prevWasDielectric && ( curLayer.Type != LAYER_TYPE::CONSTRUCTION ) )
        {
            stackup.Add( tempKiCadLayer ); //only add dielectric layers here after all are done
            dielectricSublayer = 0;
            prevWasDielectric  = false;
            noOfKiCadStackupLayers++;
        }

        switch( curLayer.Type )
        {
        case LAYER_TYPE::ALLDOC:
        case LAYER_TYPE::ALLELEC:
        case LAYER_TYPE::ALLLAYER:
        case LAYER_TYPE::ASSCOMPCOPP:
        case LAYER_TYPE::NOLAYER:
            //Shouldn't be here if CPA file is correctly parsed and not corrupt
            THROW_IO_ERROR( wxString::Format(
                    _( "Unexpected layer '%s' in layer stack." ), curLayer.Name ) );
            continue;
        case LAYER_TYPE::JUMPERLAYER:
            copperType     = LAYER_T::LT_JUMPER;
            kicadLayerID   = getKiCadCopperLayerID( ++numElecAndPowerLayers );
            kicadLayerType = BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_COPPER;
            layerTypeName  = KEY_COPPER;
            break;
        case LAYER_TYPE::ELEC:
            copperType     = LAYER_T::LT_SIGNAL;
            kicadLayerID   = getKiCadCopperLayerID( ++numElecAndPowerLayers );
            kicadLayerType = BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_COPPER;
            layerTypeName  = KEY_COPPER;
            break;
        case LAYER_TYPE::POWER:
            copperType     = LAYER_T::LT_POWER;
            kicadLayerID   = getKiCadCopperLayerID( ++numElecAndPowerLayers );
            kicadLayerType = BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_COPPER;
            layerTypeName  = KEY_COPPER;
            break;
        case LAYER_TYPE::CONSTRUCTION:
            kicadLayerID      = PCB_LAYER_ID::UNDEFINED_LAYER;
            kicadLayerType    = BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_DIELECTRIC;
            prevWasDielectric = true;
            layerTypeName     = KEY_PREPREG;
            //TODO handle KEY_CORE and KEY_PREPREG
            //will need to look at CADSTAR layer embedding (CPA_LAYER->Embedding)
            //check electrical layers above and below to decide if current layer is prepreg
            // or core
            break;
        case LAYER_TYPE::DOC:
            //TODO find out a suitable KiCad Layer alternative for this CADSTAR type
            continue; //ignore
        case LAYER_TYPE::NONELEC:
            switch( curLayer.SubType )
            {
            case LAYER_SUBTYPE::LAYERSUBTYPE_ASSEMBLY:
            case LAYER_SUBTYPE::LAYERSUBTYPE_NONE:
            case LAYER_SUBTYPE::LAYERSUBTYPE_PLACEMENT:
                //TODO find out a suitable KiCad Layer alternative for these CADSTAR types
                continue; //ignore these layer types for now
            case LAYER_SUBTYPE::LAYERSUBTYPE_PASTE:
                kicadLayerType = BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_SOLDERPASTE;
                if( numElecAndPowerLayers > 0 )
                {
                    kicadLayerID  = PCB_LAYER_ID::F_Paste;
                    layerTypeName = _HKI( "Top Solder Paste" );
                }
                else
                {
                    kicadLayerID  = PCB_LAYER_ID::B_Paste;
                    layerTypeName = _HKI( "Bottom Solder Paste" );
                }
                break;
            case LAYER_SUBTYPE::LAYERSUBTYPE_SILKSCREEN:
                kicadLayerType = BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_SILKSCREEN;
                if( numElecAndPowerLayers > 0 )
                {
                    kicadLayerID  = PCB_LAYER_ID::F_SilkS;
                    layerTypeName = _HKI( "Top Silk Screen" );
                }
                else
                {
                    kicadLayerID  = PCB_LAYER_ID::B_SilkS;
                    layerTypeName = _HKI( "Bottom Silk Screen" );
                }
                break;
            case LAYER_SUBTYPE::LAYERSUBTYPE_SOLDERRESIST:
                kicadLayerType = BOARD_STACKUP_ITEM_TYPE::BS_ITEM_TYPE_SOLDERMASK;
                if( numElecAndPowerLayers > 0 )
                {
                    kicadLayerID  = PCB_LAYER_ID::F_Mask;
                    layerTypeName = _HKI( "Top Solder Mask" );
                }
                else
                {
                    kicadLayerID  = PCB_LAYER_ID::B_Mask;
                    layerTypeName = _HKI( "Bottom Solder Mask" );
                }
                break;
            default:
                wxASSERT_MSG( true, wxT( "Unknown CADSTAR Layer Sub-type" ) );
                break;
            }
            break;
        default:
            wxASSERT_MSG( true, wxT( "Unknown CADSTAR Layer Type" ) );
            break;
        }

        if( dielectricSublayer == 0 )
            tempKiCadLayer = new BOARD_STACKUP_ITEM( kicadLayerType );

        tempKiCadLayer->SetLayerName( curLayer.Name );
        tempKiCadLayer->SetBrdLayerId( kicadLayerID );

        if( prevWasDielectric )
        {
            wxASSERT_MSG( kicadLayerID == PCB_LAYER_ID::UNDEFINED_LAYER,
                    wxT( "Error Processing Dielectric Layer. "
                         "Expected to have undefined layer type" ) );

            if( dielectricSublayer == 0 )
                tempKiCadLayer->SetDielectricLayerId( ++numDielectricLayers );
            else
                tempKiCadLayer->AddDielectricPrms( dielectricSublayer );
        }

        if( curLayer.MaterialId != UNDEFINED_MATERIAL_ID )
        {
            tempKiCadLayer->SetMaterial(
                    cpaMaterials[curLayer.MaterialId].Name, dielectricSublayer );
            tempKiCadLayer->SetEpsilonR( cpaMaterials[curLayer.MaterialId].Permittivity.GetDouble(),
                    dielectricSublayer );
            tempKiCadLayer->SetLossTangent(
                    cpaMaterials[curLayer.MaterialId].LossTangent.GetDouble(), dielectricSublayer );
            //TODO add Resistivity when KiCad supports it
        }

        tempKiCadLayer->SetThickness(
                curLayer.Thickness * KiCadUnitMultiplier, dielectricSublayer );

        wxASSERT( layerTypeName != wxEmptyString );
        tempKiCadLayer->SetTypeName( layerTypeName );

        if( !prevWasDielectric )
        {
            stackup.Add( tempKiCadLayer ); //only add non-dielectric layers here
            ++noOfKiCadStackupLayers;
            layerIDs.push_back( tempKiCadLayer->GetBrdLayerId() );
            designSettings.SetEnabledLayers( LSET( &layerIDs[0], layerIDs.size() ) );
        }
        else
            ++dielectricSublayer;

        if( copperType != LAYER_T::LT_UNDEFINED )
        {
            wxASSERT( mBoard->SetLayerType( tempKiCadLayer->GetBrdLayerId(),
                    copperType ) ); //move to outside, need to enable layer in board first
            lastElectricalLayerIndex = noOfKiCadStackupLayers - 1;
            wxASSERT( mBoard->SetLayerName(
                    tempKiCadLayer->GetBrdLayerId(), tempKiCadLayer->GetLayerName() ) );
            //TODO set layer names for other CADSTAR layers when KiCad supports custom
            //layer names on non-copper layers
            mCopperLayers.insert( std::make_pair( curLayer.PhysicalLayer, curLayer.ID ) );
        }
        //TODO map kicad layer to CADSTAR layer in mLayermap
    }

    //change last copper layer to be B_Cu instead of an inner layer
    PCB_LAYER_ID lastElecBrdId =
            stackup.GetStackupLayer( lastElectricalLayerIndex )->GetBrdLayerId();
    std::remove( layerIDs.begin(), layerIDs.end(), lastElecBrdId );
    layerIDs.push_back( PCB_LAYER_ID::B_Cu );
    tempKiCadLayer = stackup.GetStackupLayer( lastElectricalLayerIndex );
    tempKiCadLayer->SetBrdLayerId( PCB_LAYER_ID::B_Cu );
    wxASSERT( mBoard->SetLayerName(
            tempKiCadLayer->GetBrdLayerId(), tempKiCadLayer->GetLayerName() ) );

    //make all layers enabled and visible
    mBoard->SetEnabledLayers( LSET( &layerIDs[0], layerIDs.size() ) );
    mBoard->SetVisibleLayers( LSET( &layerIDs[0], layerIDs.size() ) );

    mBoard->SetCopperLayerCount( numElecAndPowerLayers );
}


void CADSTAR_PCB::loadBoards()
{
    for( std::pair<BOARD_ID, BOARD> boardPair : Layout.Boards )
    {
        BOARD& board = boardPair.second;
        drawCadstarShape( board.Shape, PCB_LAYER_ID::Edge_Cuts, board.LineCodeID );

        //TODO process board attributes
        //TODO process addition to a group
    }
}


void CADSTAR_PCB::drawCadstarShape( const SHAPE& aCadstarShape, const PCB_LAYER_ID& aKiCadLayer,
        const LINECODE_ID& aCadstarLinecodeID )
{
    int lineThickness = getLineThickness( aCadstarLinecodeID );

    switch( aCadstarShape.Type )
    {
    case SHAPE_TYPE::OPENSHAPE:
    case SHAPE_TYPE::OUTLINE:
        drawCadstarVerticesAsSegments( aCadstarShape.Vertices, aKiCadLayer, lineThickness );
        drawCadstarCutoutsAsSegments( aCadstarShape.Cutouts, aKiCadLayer, lineThickness );
        break;

    case SHAPE_TYPE::SOLID:
        //TODO
        break;

    case SHAPE_TYPE::HATCHED:
        //TODO
        break;
    }
}


void CADSTAR_PCB::drawCadstarCutoutsAsSegments( const std::vector<CUTOUT>& aCutouts,
        const PCB_LAYER_ID& aKiCadLayer, const int& aLineThickness )
{
    for( CUTOUT cutout : aCutouts )
    {
        drawCadstarVerticesAsSegments( cutout.Vertices, aKiCadLayer, aLineThickness );
    }
}


void CADSTAR_PCB::drawCadstarVerticesAsSegments( const std::vector<VERTEX>& aCadstarVertices,
        const PCB_LAYER_ID& aKiCadLayer, const int& aLineThickness )
{
    std::vector<DRAWSEGMENT*> drawSegments = getDrawSegments( aCadstarVertices );

    for( DRAWSEGMENT* ds : drawSegments )
    {
        ds->SetWidth( aLineThickness );
        ds->SetLayer( aKiCadLayer );
        mBoard->Add( ds, ADD_MODE::APPEND );
    }
}

std::vector<DRAWSEGMENT*> CADSTAR_PCB::getDrawSegments(
        const std::vector<VERTEX>& aCadstarVertices )
{
    std::vector<DRAWSEGMENT*> drawSegments;

    if( aCadstarVertices.size() < 2 )
        //need at least two points to draw a segment! (unlikely but possible to have only one)
        return drawSegments;

    const VERTEX* prev = &aCadstarVertices.at( 0 ); // first one should always be a point vertex
    double        arcStartAngle, arcEndAngle, arcAngle;
    bool          cw = false;

    for( size_t i = 1; i < aCadstarVertices.size(); i++ )
    {
        const VERTEX* cur = &aCadstarVertices[i];
        DRAWSEGMENT*  ds  = new DRAWSEGMENT( mBoard );
        cw                = false;

        wxPoint startPoint = getKiCadPoint( prev->End );
        wxPoint endPoint   = getKiCadPoint( cur->End );
        wxPoint centerPoint;

        if( cur->Type == VERTEX_TYPE::ANTICLOCKWISE_SEMICIRCLE
                || cur->Type == VERTEX_TYPE::CLOCKWISE_SEMICIRCLE )
            centerPoint = ( startPoint + endPoint ) / 2;
        else
            centerPoint = getKiCadPoint( cur->Center );

        switch( cur->Type )
        {

        case VERTEX_TYPE::POINT:
            ds->SetShape( STROKE_T::S_SEGMENT );
            ds->SetStart( startPoint );
            ds->SetEnd( endPoint );
            break;

        case VERTEX_TYPE::CLOCKWISE_SEMICIRCLE:
        case VERTEX_TYPE::CLOCKWISE_ARC:
            cw = true;
        case VERTEX_TYPE::ANTICLOCKWISE_SEMICIRCLE:
        case VERTEX_TYPE::ANTICLOCKWISE_ARC:
            ds->SetShape( STROKE_T::S_ARC );
            ds->SetArcStart( startPoint );
            ds->SetCenter( centerPoint );

            arcStartAngle = getPolarAngle( startPoint - centerPoint );
            arcEndAngle   = getPolarAngle( endPoint - centerPoint );
            arcAngle      = arcEndAngle - arcStartAngle;
            //TODO: detect if we are supposed to draw a circle instead (i.e. when two SEMICIRCLE with oppositve state/end points)

            if( cw )
                ds->SetAngle( NormalizeAnglePos( arcAngle ) );
            else
                ds->SetAngle( NormalizeAngleNeg( arcAngle ) );
            break;
        }

        drawSegments.push_back( ds );
        prev = cur;
    }

    return drawSegments;
}


int CADSTAR_PCB::getLineThickness( const LINECODE_ID& aCadstarLineCodeID )
{
    if( Assignments.Codedefs.LineCodes.find( aCadstarLineCodeID )
            == Assignments.Codedefs.LineCodes.end() )
        return mBoard->GetDesignSettings().GetLineThickness( PCB_LAYER_ID::Edge_Cuts );
    else
        return Assignments.Codedefs.LineCodes[aCadstarLineCodeID].Width * KiCadUnitMultiplier;
}


wxPoint CADSTAR_PCB::getKiCadPoint( wxPoint aCadstarPoint )
{
    wxPoint retval;

    retval.x = ( aCadstarPoint.x - mDesignCenter.x ) * KiCadUnitMultiplier;
    retval.y = -( aCadstarPoint.y - mDesignCenter.y ) * KiCadUnitMultiplier;

    return retval;
}


double CADSTAR_PCB::getPolarAngle( wxPoint aPoint )
{
  
    return NormalizeAnglePos( ArcTangente( aPoint.y, aPoint.x ) );
}


PCB_LAYER_ID CADSTAR_PCB::getKiCadCopperLayerID( unsigned int aLayerNum )
{
    switch( aLayerNum )
    {
    case 1:
        return PCB_LAYER_ID::F_Cu;
    case 2:
        return PCB_LAYER_ID::In1_Cu;
    case 3:
        return PCB_LAYER_ID::In2_Cu;
    case 4:
        return PCB_LAYER_ID::In3_Cu;
    case 5:
        return PCB_LAYER_ID::In4_Cu;
    case 6:
        return PCB_LAYER_ID::In5_Cu;
    case 7:
        return PCB_LAYER_ID::In6_Cu;
    case 8:
        return PCB_LAYER_ID::In7_Cu;
    case 9:
        return PCB_LAYER_ID::In8_Cu;
    case 10:
        return PCB_LAYER_ID::In9_Cu;
    case 11:
        return PCB_LAYER_ID::In10_Cu;
    case 12:
        return PCB_LAYER_ID::In11_Cu;
    case 13:
        return PCB_LAYER_ID::In12_Cu;
    case 14:
        return PCB_LAYER_ID::In13_Cu;
    case 15:
        return PCB_LAYER_ID::In14_Cu;
    case 16:
        return PCB_LAYER_ID::In15_Cu;
    case 17:
        return PCB_LAYER_ID::In16_Cu;
    case 18:
        return PCB_LAYER_ID::In17_Cu;
    case 19:
        return PCB_LAYER_ID::In18_Cu;
    case 20:
        return PCB_LAYER_ID::In19_Cu;
    case 21:
        return PCB_LAYER_ID::In20_Cu;
    case 22:
        return PCB_LAYER_ID::In21_Cu;
    case 23:
        return PCB_LAYER_ID::In22_Cu;
    case 24:
        return PCB_LAYER_ID::In23_Cu;
    case 25:
        return PCB_LAYER_ID::In24_Cu;
    case 26:
        return PCB_LAYER_ID::In25_Cu;
    case 27:
        return PCB_LAYER_ID::In26_Cu;
    case 28:
        return PCB_LAYER_ID::In27_Cu;
    case 29:
        return PCB_LAYER_ID::In28_Cu;
    case 30:
        return PCB_LAYER_ID::In29_Cu;
    case 31:
        return PCB_LAYER_ID::In30_Cu;
    case 32:
        return PCB_LAYER_ID::B_Cu;
    }
    return PCB_LAYER_ID::UNDEFINED_LAYER;
}


PCB_LAYER_ID CADSTAR_PCB::getKiCadLayer( const LAYER_ID& aCadstarLayerID )
{
    if( mLayermap.find( aCadstarLayerID ) == mLayermap.end() )
        return PCB_LAYER_ID::Cmts_User; // TODO need to handle ALLELEC, ALLLAYER, ALLDOC,
                                        // etc. For now just put unmapped layers here
    else
        return mLayermap[aCadstarLayerID];
}